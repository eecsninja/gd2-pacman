#include <EEPROM.h>
#include <SPI.h>
#include <GD2.h>

#include "pacman_assets.h"

void setup()
{
  GD.begin();
  LOAD_ASSETS();
}

#define KEY_RIGHT   0x01
#define KEY_LEFT    0x02
#define KEY_UP      0x04
#define KEY_DOWN    0x08

#define KEY_COIN    0x10
#define KEY_START   0x20

#define KEY_DIR (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)
#define KEY_ANY (KEY_DIR | KEY_COIN | KEY_START)

#define SPRITE_COUNT 5

#define MODE0_COLUMNS           19
#define MODE0_ROWS              24
static uint8_t tilemap[MODE0_COLUMNS * MODE0_ROWS];

#define mode0_pgmmap(x)      (x)
#define mode0_get_block(x,y) (tilemap[(x)+(y)*MODE0_COLUMNS])
#define mode0_set_block(x,y, blk) do {tilemap[(x)+(y)*MODE0_COLUMNS] = (blk);} while(0)
#define mode0_cls()  memset(&mode0_get_block(0,0), ' ', MODE0_ROWS * MODE0_COLUMNS)

#define DIV10(x)  ((x)/10)
#define DIV8(x)   ((x)/8)

#define mode0_is_rammap(x)   ((x) >= 240)
#define free_space(x,y) ( ((mode0_get_block(DIV8(x), DIV10(y)) & 0xf0) == 0x20) || \
	mode0_is_rammap(mode0_get_block(DIV8(x), DIV10(y))))

static uint8_t key_read(uint8_t mask)
{
  return mask & KEY_LEFT;
}

static void utoa10(uint16_t n, uint8_t *d)
{
  *d++ = ('0' + (n / 10000) % 10);   // ten-thousands
  *d++ = ('0' + (n / 1000) % 10);    // thousands
  *d++ = ('0' + (n / 100) % 10);     // hundreds
  *d++ = ('0' + (n / 10) % 10);      // tens
  *d++ = ('0' + n % 10);             // ones
}

static void sound_play(uint8_t *c)
{
}

static uint8_t misc_rand()
{
  return GD.random() & 0xff;
}

static void sync()
{
  delay(17);
}

#define mode0_print(x, y, txt) \
({ \
  static char __c[] PROGMEM = (txt); \
  memcpy_P(&mode0_get_block((x),(y)), (__c), strlen_P(__c)); \
})

static void draw_sprite(byte x, byte y, byte c)
{
  GD.Vertex2ii(164 + x, y, FONT_HANDLE, c);
}

static void paint_tilemap()
{
  GD.ClearColorRGB(0x000000);
  GD.Clear();
  GD.Begin(BITMAPS);
  GD.get_inputs();

  uint8_t *pblk = &mode0_get_block(0, 0);
  for (byte y = 0; y < 10 * MODE0_ROWS; y += 10) {
    for (byte x = 0; x < 8 * MODE0_COLUMNS; x += 8) {
      uint8_t c = *pblk++;
      if (c)
        GD.Vertex2ii(164 + x, y, FONT_HANDLE, c);
    }
  }
}

static void key_wait_timeout(uint8_t mask, uint8_t timeout)
{
  delay(16 * timeout);
}

/*
 *  Albert Seward Copyright (C) 2005
 *  All Rights Reserved. Proprietary.
 *  alse7905@student.uu.se
 * 
 */

#include "pacman.h"

const char pacman_board[] PROGMEM =  PACMAN_BOARD;

void scene_intro(void)
{
  uint8_t  i, mode, tmp;
  int pos, pacman;
  
  pos = 60;
  pacman = 70;

  mode = 1;

  for (;;) {
    mode0_cls();
    mode0_print(5,1, "\033\021\021\021\021\021\021\021\032");
    mode0_print(5,2, "\020" PACMAN_LOGO "\020");
    mode0_print(5,3, "\035\021\021\021\021\021\021\021\034");

    mode0_print(4,19, "INSERT COIN");
    mode0_print(3,22, "COPYRIGHT 2005");
    mode0_print(3,23, "ALBERT SEWARD");

    for (;;) {
      paint_tilemap();

      for(i = 1; i < SPRITE_COUNT; i++) {
        tmp = pos - i * 10;
        if (tmp < 152 && tmp > 0) {
          if (mode)
            draw_sprite( tmp, 70, BLINKY -1 + i);
          else if(pacman < tmp)
            draw_sprite( tmp, 70, EATEN);
          else
            draw_sprite( tmp, 70, EATABLE);
        }
      }

      if(pacman < 152 && pacman > 0)
        draw_sprite( pacman, 70, pacman & 8 ? (mode?PACMAN_OPEN_RIGHT:PACMAN_OPEN_LEFT) : (mode?PACMAN_CLOSED_RIGHT:PACMAN_CLOSED_LEFT));

      if(mode) {
        pos++;
        pacman++;
      } else {
        pos--;
        pacman-=2;
      }

      if(pos > 152)
        pacman = 200;
      else if(pos < -10)
        pacman = 0;


      if(pos > 250) {
        mode = 0;
      } else if(pos < -40) {
        mode0_print(5,6, "\045  BLINKY");
        key_wait_timeout(KEY_COIN, 60);
        mode0_print(5,7, "\046  CLYDE");
        key_wait_timeout(KEY_COIN, 60);
        mode0_print(5,8, "\047  PINKY");
        key_wait_timeout(KEY_COIN, 60);
        mode0_print(5,9, "\050  INKEY");
        key_wait_timeout(KEY_COIN, 200);
        // memset(&mode0_get_block(5,6), ' ', 20*4);
        break;
      }
      GD.swap();
    }
  }

  // highscore_view();
}

/*
 *  Albert Seward Copyright (C) 2005
 *  All Rights Reserved. Proprietary.
 *  alse7905@student.uu.se
 * 
 */

void pacman_ghost_AI(character_t *chr);
static inline void pacman_move(character_t *chr);
static inline uint8_t pacman_play_level(uint8_t level, uint16_t *score);
static inline void pacman_level_init(void);


void pacman_flash_text(char *str, uint8_t key_mask);

struct {
  uint8_t speed:4;
  uint8_t count:4;
} move[5];

uint8_t PACMAN_ICO[4][2] PROGMEM = {
  {PACMAN_CLOSED_RIGHT, PACMAN_OPEN_RIGHT},
  {PACMAN_CLOSED_LEFT, PACMAN_OPEN_LEFT},
  {PACMAN_CLOSED_DOWN, PACMAN_OPEN_DOWN},
  {PACMAN_CLOSED_UP, PACMAN_OPEN_UP},
};

struct {
                  uint8_t ico;
                  uint8_t x, y;
}
START_POSITION[] PROGMEM = {
  {PACMAN_CLOSED_LEFT, 72, 210},
  {BLINKY, 112, 50},
  {CLYDE, 48, 110},
  {PINKY, 24, 20},
  {INKEY, 96, 100}, 
  {APPLE, 1, 2},
  {BANANA, 17, 2},
  {PEAR, 1, 21},
  {CHERRY, 17, 21},
};


uint8_t sound_fruit[] PROGMEM = {
  11,
  13, 4, 
  14, 2,
  15, 2,
  13, 4,
  0
};

uint8_t sound_dot[] PROGMEM = {
  50,
  56,
  50,
  0
};

uint8_t sound_eat_ghost[] PROGMEM = {
  36,2,
  40,3,
  0
};

uint8_t sound_bonus[] PROGMEM = {
  53,
  53,
  60,
  60,
  0,
};


uint8_t sound_level_done[] PROGMEM = {
  46,
  46,
  46,
  50,
  50,
  0,
};

uint8_t sound_die[] PROGMEM = {
  40,
  40,
  50,
  50,
  60,
  60,
  70,
  70,
  80,
  80,
  80,
  80,
  0,
};

void pacman_flash_text(char *str, uint8_t key_mask)
{ 
  char tmp[9];
  memcpy(tmp, &mode0_get_block(5,11), 9);
  memcpy_P(&mode0_get_block(5,11), str, strlen_P(str));
  paint_tilemap(); GD.swap();
  key_wait_timeout(key_mask, 255); 
  memcpy(&mode0_get_block(5,11), tmp, 9);
}

int game_main(void)
{
  uint8_t i;
  uint8_t lives, level, credits;
  uint16_t score;

#if 0
  if(boot_test() == EEPROM_ERROR)
    highscore_reset();

  mode0_print(1,7, "PAL");

  /* PAL/NTSC select
  i = KEY_RIGHT;
  do {    
    if(i == KEY_LEFT) {
      mode0_print(1,7, "NTSC");
      video_init(NTSC, mode0);
    } else if(i == KEY_RIGHT) {
      mode0_print(1,7, "PAL ");
      video_init(PAL, mode0);
    }
    i = key_read(KEY_ANY);
  } while(i != KEY_START);
  */

  sound_init();

    while(!key_read(KEY_ANY));
#endif
  
  // Main game loop
  for(;;) {
    // scene_intro();
    credits = 1;
    score = 0;
    lives = 3;
    level = 1;        

    while(lives) {
    
      // New game
      pacman_level_init();      
      mode0_print(1,0,"CREDITS "); mode0_set_block(9,0, credits + '0');
      mode0_print(11,0,"LEVEL "); mode0_set_block(17,0, level + '0');
      utoa10(score, &mode0_get_block(13,23));
      for(i = 0; i < 4; i++)
        mode0_set_block(1+i, 23, i<(lives-1)?mode0_pgmmap(HART):' ');
      
      // Play level
      while(lives) {  
        pacman_flash_text(PSTR("GET READY"), KEY_START);

        if(pacman_play_level(level, &score))
          break;
        lives--;
        for(i = 0; i < 4; i++)
          mode0_set_block(1+i, 23, i<(lives-1)?mode0_pgmmap(HART):' ');
      }

      // Level done
      if (lives) {                  
        mode0_print(5,11, "LEVEL DONE");
        mode0_print(7,13, "BONUS");
        for(i = 1; i <= 200; i++) {
          if(!(i % 8))
            sound_play((uint8_t *)sound_bonus);
          sync();
          utoa10(i * 5 * level, &mode0_get_block(7,14));
          
        }
        score += level * 1000;
        while(key_read(KEY_START));
        while(!key_read(KEY_START));
        level++;
      } else credits--;     
    }
    
    // Game over    
    mode0_print(4,13, "INSERT COIN");
    for(i = 0; (i < 255) && !key_read(KEY_COIN|KEY_START); i++) {
      if(i & 16) mode0_print(5,11, "  \020   \020  ");
      else mode0_print(5,11, "GAME OVER");
      paint_tilemap();
      sync(); sync(); sync();
    }
    while(key_read(KEY_COIN|KEY_START));
    // highscore_enter(score);           
  }
  
  return 0;
 
}


#define MOD10(x)    ((x) % 10)
#define MOD8(x)     ((x) % 8)

// This AI SUXX! Rewrite
void pacman_ghost_AI(character_t *chr) { 
  uint8_t i;
  uint8_t x, y; 
            
  for(i = 1; i < SPRITE_COUNT; i++) {
    if (!MOD10(chr[i].y) && !MOD8(chr[i].x)) {
      
      if(chr[i].ico == EATEN) {
        x = 9*8;
        y = 110;
      } else  {
        x = chr[0].x;
        y = chr[0].y;
        if(chr[i].x == 9*8 && chr[i].y == 110)
          y = 50;
      } 
      
      
      if(i == 3 || chr[i].ico == EATEN) {
        if(move[i].count == 0) {
          if(x < chr[i].x && chr[i].move != DIR_RIGHT && free_space(chr[i].x-8, chr[i].y)) 
            chr[i].dirw = DIR_LEFT;
          else if(x > chr[i].x && chr[i].move != DIR_LEFT && free_space(chr[i].x+8, chr[i].y)) 
            chr[i].dirw = DIR_RIGHT;           
          else if(y < chr[i].y && chr[i].move != DIR_DOWN && free_space(chr[i].x, chr[i].y-10))
            chr[i].dirw = DIR_UP;
          else if(y > chr[i].y && chr[i].move != DIR_UP && free_space(chr[i].x, chr[i].y+10)) 
            chr[i].dirw = DIR_DOWN;
          else chr[i].dirw = misc_rand() & 3;
        } 
      } else {
        if(misc_rand() % 3) {
          if(x < chr[i].x) chr[i].dirw = DIR_LEFT;
          else if(x > chr[i].x) chr[i].dirw = DIR_RIGHT;
          
          if(y < chr[i].y) chr[i].dirw = DIR_UP;
          else if(y > chr[i].y) chr[i].dirw = DIR_DOWN;
        } else {
          if(y < chr[i].y) chr[i].dirw = DIR_UP;
          else if(y > chr[i].y) chr[i].dirw = DIR_DOWN;
          
          if(x < chr[i].x) chr[i].dirw = DIR_LEFT;
          else if(x > chr[i].x) chr[i].dirw = DIR_RIGHT;
        }
      }
      
      // Escape!
      if(chr[i].ico == EATABLE) {
        chr[i].dirw++;
        chr[i].dirw %= 4;
      }     
      
      if(chr[i].dirw == DIR_DOWN) {
        if(free_space(chr[i].x, chr[i].y+10))
          chr[i].move = DIR_DOWN;
        else if(free_space(chr[i].x+8, chr[i].y) && chr[i].move != DIR_LEFT )
          chr[i].move = DIR_RIGHT;
        else if(free_space(chr[i].x-8, chr[i].y) && chr[i].move != DIR_RIGHT )
          chr[i].move = DIR_LEFT;
        else
          chr[i].move = DIR_PAUSE;
              
      } else if(chr[i].dirw == DIR_UP) {
        if(free_space(chr[i].x, chr[i].y-10))
          chr[i].move = DIR_UP;
        else if(free_space(chr[i].x+8, chr[i].y) && chr[i].move != DIR_LEFT )
          chr[i].move = DIR_RIGHT;
        else if(free_space(chr[i].x-8, chr[i].y) && chr[i].move != DIR_RIGHT )
          chr[i].move = DIR_LEFT;
        else
          chr[i].move = DIR_PAUSE;
        
        
      } else if(chr[i].dirw == DIR_LEFT) {
        if(free_space(chr[i].x-8, chr[i].y)) 
          chr[i].move = DIR_LEFT;
        else if(free_space(chr[i].x, chr[i].y-10) && chr[i].move != DIR_DOWN ) 
          chr[i].move = DIR_UP;
        else if(free_space(chr[i].x, chr[i].y+10) && chr[i].move != DIR_UP )
          chr[i].move = DIR_DOWN;
        else
          chr[i].move = DIR_PAUSE;
        
        
      } else if(chr[i].dirw == DIR_RIGHT) {
        if(free_space(chr[i].x+8, chr[i].y))
          chr[i].move = DIR_RIGHT;
        else if(free_space(chr[i].x, chr[i].y+10) && chr[i].move != DIR_UP )
          chr[i].move = DIR_DOWN;
        else if(free_space(chr[i].x, chr[i].y-10) && chr[i].move != DIR_DOWN )
          chr[i].move = DIR_UP;
        else
          chr[i].move = DIR_PAUSE;
        
      }
    }
  }
}

    

static inline void pacman_move(character_t *chr) {
  uint8_t i;
  
  // Make controls easier
  if(!MOD10(chr[0].y) && !MOD8(chr[0].x)) {
    if(chr[0].dirw == DIR_RIGHT && free_space(chr[0].x+8, chr[0].y))
      chr[0].move = DIR_RIGHT;
    else if(chr[0].dirw == DIR_LEFT && free_space(chr[0].x-8, chr[0].y))
      chr[0].move = DIR_LEFT;
    else if(chr[0].dirw == DIR_DOWN && free_space(chr[0].x, chr[0].y+10))
      chr[0].move = DIR_DOWN;
    else if(chr[0].dirw == DIR_UP && free_space(chr[0].x, chr[0].y-10))
      chr[0].move = DIR_UP;
  } 
  
  for(i = 0; i < SPRITE_COUNT; i++) {
    
    // Tunnel warp
    if(chr[i].y == 110) {
      if(chr[i].x == 1)
        chr[i].x = 8*18-2;
      else if(chr[i].x == 18*8-1)
        chr[i].x = 2;
    }
    
    // Do the move
    if(move[i].count++ == move[i].speed) {
      if(chr[i].move == DIR_RIGHT && 
         (MOD8(chr[i].x) || free_space(chr[i].x+8, chr[i].y)))        
        chr[i].x++;
      else if(chr[i].move == DIR_LEFT && 
        (MOD8(chr[i].x) || free_space(chr[i].x-8, chr[i].y)))
        chr[i].x--;
      else if(chr[i].move == DIR_DOWN && 
        (MOD10(chr[i].y) || free_space(chr[i].x, chr[i].y+10)))
        chr[i].y+=2;
      else if(chr[i].move == DIR_UP &&
        (MOD10(chr[i].y)  || free_space(chr[i].x, chr[i].y-10)) )
        chr[i].y-=2;
      move[i].count = 0;
    }
  }
  
}

// Total amount dots to eat (including superdots)
uint8_t dots_left;
  

static inline void pacman_level_init(void) {  
  uint8_t i;
  mode0_cls();
  
  // Total amount dots to eat (includeing superdots)
  dots_left = 184;
  
  // Game board
  memcpy_P(&mode0_get_block(0,1), pacman_board, 22 * MODE0_COLUMNS);      
  mode0_print(5,23, PACMAN_LOGO);
  
  // Place fruit
  for(i = 5; i < 9; i++)
    mode0_set_block(pgm_read_byte(&START_POSITION[i].x), 
        pgm_read_byte(&START_POSITION[i].y),
        pgm_read_byte(&START_POSITION[i].ico));

}

#define BLINK_TIME 60
#define SHOW_BONUS_TIME 70

struct {
  uint8_t pacman_speed;
  uint8_t blinky_speed;
  uint8_t clyde_speed;
  uint8_t pinky_speed;
  uint8_t inkey_speed;
  uint8_t blue_time;
}
level_data[] PROGMEM = {
  {1,3,4,4,4, 250},
  {1,2,3,3,3, 200},
  {1,1,2,2,2, 150},   
  {1,1,2,2,2, 100},   
  {1,1,2,2,2, 50},    
  {1,1,2,2,2, 3},   
};

static inline uint8_t pacman_play_level(uint8_t level, uint16_t *score)
{
  uint16_t timer; 
  unsigned char i;
  character_t chr[SPRITE_COUNT];
  uint16_t blue_time; 
  
  // Place Mr Pacman and ghosts
  chr[0].move = DIR_UP;
  for(i = 0; i < SPRITE_COUNT; i++) {
    chr[i].ico = pgm_read_byte(&START_POSITION[i].ico);
    chr[i].x = pgm_read_byte(&START_POSITION[i].x);
    chr[i].y = pgm_read_byte(&START_POSITION[i].y);
  }
  
  // Set level properties 
  level--;
  if(level>5) level = 5;  
  move[0].speed = pgm_read_byte(&level_data[level].pacman_speed);
  move[1].speed = pgm_read_byte(&level_data[level].blinky_speed);
  move[2].speed = pgm_read_byte(&level_data[level].clyde_speed);
  move[3].speed = pgm_read_byte(&level_data[level].pinky_speed);
  move[4].speed = pgm_read_byte(&level_data[level].inkey_speed);
  blue_time = pgm_read_byte(&level_data[level].blue_time);
  
  // No dots where Mr Pacman starts
  mode0_set_block(DIV8(chr[0].x), DIV10(chr[0].y), DOT0);
    
  timer = 0;
  
  // Game loop
  for(;;) {
  
    // Read keys    
    switch(key_read(KEY_DIR)) {
    case KEY_RIGHT: chr[0].dirw = DIR_RIGHT; break;       
    case KEY_LEFT: chr[0].dirw = DIR_LEFT; break;
    case KEY_UP: chr[0].dirw = DIR_UP; break;
    case KEY_DOWN: chr[0].dirw = DIR_DOWN; break;
    }       
            
    // Ghost AI
    pacman_ghost_AI(chr);
    
    // Move characters
    pacman_move(chr);
                   
    // Print score
    utoa10(*score, &mode0_get_block(13,23));
        
    // Mr Pacman eat dots
    switch(mode0_get_block(DIV8(chr[0].x), DIV10(chr[0].y))) {
    case DOT3:
      if(chr[0].move == DIR_LEFT)
        mode0_set_block(DIV8(chr[0].x), DIV10(chr[0].y), DOT1);
      else if(!MOD10(chr[0].y)) {
        mode0_set_block(DIV8(chr[0].x), DIV10(chr[0].y), DOT0);
        *score+=15;       
        dots_left--;        
        sound_play((uint8_t *)sound_dot);

      }   
      break;
    case DOT1:
    case DOT2:
      if(!MOD8(chr[0].x)) {
        mode0_set_block(DIV8(chr[0].x), DIV10(chr[0].y), DOT0);
        *score+=15;
        dots_left--;
        sound_play((uint8_t *)sound_dot);
      }     
      break;
    case APPLE:
    case BANANA: 
    case PEAR: 
    case CHERRY:
      sound_play((uint8_t *)sound_fruit);
      dots_left--;
      *score+=100;
      mode0_set_block(DIV8(chr[0].x), DIV10(chr[0].y), POINTS100);
      timer = 0xffff;
    }   
          
    
    if(timer) {
      // Toggle 100 points icon
      // TODO: There is a bug here!
                        timer--;
      for(i = 5; i < 9; i++) {
        uint8_t *p = &mode0_get_block(pgm_read_byte(&START_POSITION[i].x), 
                    pgm_read_byte(&START_POSITION[i].y));
        if((*p & 0xfe) == POINTS100) {
          if(timer & 8)
            *p ^= 1;
          if(timer < 0xffff-SHOW_BONUS_TIME)
            *p = DOT0;
        }
      }
      
      // Blue mode
      for(i = 1; i < SPRITE_COUNT; i++) {
        if(timer == 0xffff-4) {       
          if(chr[i].ico == EATABLE + i)
            chr[i].ico = EATABLE;
        } else if(chr[i].ico == EATABLE && timer == 0xffff - blue_time*2)
            chr[i].ico = EATABLE + i;
      }
          
    }
    
    // Level done?
    if(dots_left == 0) {
      sound_play((uint8_t *)sound_level_done);
      return 1; 
    }
         
    // Make Mr Pacman chew!
    chr[0].ico = pgm_read_byte(&PACMAN_ICO[chr[0].move][((chr[0].x + chr[0].y) & 8) == 0]);   
        
    paint_tilemap();
    // Draw sprites
    for(i = 0; i < SPRITE_COUNT; i++) {
      if((chr[i].ico == EATABLE) && (timer < 0xffff - (blue_time*2 - BLINK_TIME)) && (timer & 8))
        draw_sprite( chr[i].x, chr[i].y, EATABLE + i);
      else
        draw_sprite( chr[i].x, chr[i].y, chr[i].ico);
    }
    GD.swap();
      
    for(i = 1; i < SPRITE_COUNT; i++) {
      // Ghosts heal in nest
      if(chr[i].y == 110 && chr[i].x > 8*8 && chr[i].x < 10*8) 
        chr[i].ico = pgm_read_byte(&START_POSITION[i].ico);
        
      // Eaten by ghost or eat ghost
      if(abs(chr[0].x - chr[i].x) < 7 && abs(chr[0].y - chr[i].y) < 9) {
        if(chr[i].ico == EATABLE)  { 
          chr[i].ico = EATEN;
          sound_play((uint8_t *)sound_eat_ghost);
          *score += 200;
        } else if(chr[i].ico != EATEN ) {
          sound_play((uint8_t *)sound_die);
          return 0;       
        } 
        
      } 
    }
  }
}

void loop()
{
  // scene_intro();
  game_main();
}

