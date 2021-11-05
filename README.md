# K-on houkago live modding tools

A library (and CLI application) for managing file types found in the PSP game [けいおん！放課後ライブ！！](https://k-on.fandom.com/wiki/K-ON!_Ho-kago_Live!!).
Currently, only a small subset of file types is supported, and some can only be converted one way.
Eventually, the goal is to fully translate the game. This is almost possible.

## Using the program
The application included is called teatimetools. Running it without any arguments will print out the help menu.
There's no GUI, so run it from a terminal window.

There is both an "advanced" operation mode, and a simple drag-n-drop interface.
for the drag-n-drop interface, it will try to deduce the operation automatically (for example, dropping a .iso file onto the executable will unpack the iso file)
for the advanced interface, read the help. (printed when running program without any arguments)