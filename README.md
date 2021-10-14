# K-on houkago live modding tools

A library (and CLI application) for managing file types found in the PSP game [けいおん！放課後ライブ！！](https://k-on.fandom.com/wiki/K-ON!_Ho-kago_Live!!).
Currently, only a small subset of file types is supported, and some can only be converted one way.
Eventually, the goal is to fully translate the game. This is almost possible.

## Using the program
The application included is called teatimetools. Running it without any arguments will print out the help menu.
There's no GUI, so run it from a terminal window.

## extracting and repackaging CPK archives
There's no built-in tools for managing CPK files *yet*, so you'll need a different program for this.
Personally, I use [YACpkTool](https://github.com/Brolijah/YACpkTool) with wine.
I'd eventually like to add built-in support for this, but it's not exactly high priority.

## Rebuilding the iso
The PSP ISO format is just a normal ISO9660-format ISO file, but with some specific values set for names and the like. As such, it's quite easy to rebuild the ISO using standard tools.
Sooner or later I'll make use of libarchive to embed an ISO patcher into the application, but for now you'll have to do it by hand.

 - for linux: **```mkisofs -iso-level 4 -xa -A "PSP GAME" -V "" -sysid "PSP GAME" -volset "" -p "" -publisher "" -o <NEW_ISO.ISO> <UNPACKED_DIRECTORY>```**
 - for windows: good luck
