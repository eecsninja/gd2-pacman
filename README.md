gd2-pacman
==========

Gameduino port of Albert Seward's AVR pacman.
To compile on Arduino, build a sketch with `pacman.ino`, `pacman.h` and `pacman_assets.h`.

The direction is controlled by tilt.  Tap to play again.

![screenshot](/screenshot.jpg)

Some notes on the conversion
----------------------------

Playing area is 19x24 tiles.
With the original 8x10 tiles, this gives an area of 152x240.
With tiles resized to 14x18 the screen area is 266x408,
a good fit for Gameduino 2's screen in sideways ("portrait") orientation.
