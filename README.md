# The unknown MUD!

To compile the mud you will need the
adevs[https://github.com/smiz/adevs] simulation
library, which you will need to build. You will also need
the yaml-cpp parser which you should be able to install with a
package manager. To build the library, go to the src director,
edit the makefile so that it points at your adevs build,
and then run `make'. If all goes well, you will have an
executable called `server'.

If you run the server with an argument, such as `server 4000'
it will start the mud on port 4000. Otherwise, the mud
is in single player mode and takes input directly from the
console.

You can generate html documentation from the source code
by running doxygen in the top level directory.

# Startup

The server loads all of the room files found in the directory
and sub-directories rooted in `rooms'. It also loads all of
the character files in the directory 'characters'. New 
characters are put in the 'characters' directory. Monsters
listed in room files are loaded from the 'monsters' directory.
Items list in room, character, or monster files are loaded
from the 'items' directory.

