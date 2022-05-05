# teatimetool

A library (and CLI application) for managing file types found in various games, primarily [けいおん！放課後ライブ！！](https://k-on.fandom.com/wiki/K-ON!_Ho-kago_Live!!).
The original goal was to be able to translate [けいおん！放課後ライブ！！](https://k-on.fandom.com/wiki/K-ON!_Ho-kago_Live!!), this is now possible ([and nearing completion](https://youtu.be/4bc0pycxbh0)).

## Using the program
The application included is called teatimetools. Running it without any arguments will print out the help menu.
There's no GUI, so run it from a terminal window.

There is both an "advanced" operation mode, and a simple drag-n-drop interface.
for the drag-n-drop interface, it will try to deduce the operation automatically (for example, dropping a .iso file onto the executable will unpack the iso file)
for the advanced interface, read the help. (printed when running program without any arguments)

## Future goals
Currently, the only way to easily apply batch operations is through a custom command list format. This is difficult to work with, and also quite limited. Rather than expanding this system, eventually I'd like to switch to using lua instead.
A massive rewrite is also needed in order to allow for better in-memory operations on archives and the like. Currently all files in an archive need to be extracted before a different command can work on them, and this results in a lot of unnecessary file writes (and thus a lot of time loss).
Lastly, said rewrite should also improve "streaming" of files, rather than relying on loading entire files into memory for operations, especially for archives.

## How to help
I do this in my free time, as a fun side-project. If you'd like to follow along, you can join the [discord server](https://youtu.be/bHY45QfGRc4), look at the videos I published about the project on [youtube](https://www.youtube.com/channel/UCi4vCtRCjlAhi7LF9OQQ3_g), or support me by [donating](https://www.buymeacoffee.com/FrainBreeze).
