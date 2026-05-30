
#ifndef _maze_h_
#define _maze_h_
#include "room.h"

#define MAZE_START_ROOM_NUMBER 9999999

/**
 * This room generates a maze.
 * It is intended to fill space
 * with something to do while real human beings create
 * actual, interesting content for the mud.
 */
class Maze: public Room {

    public:

    Maze(Graph& graph, int exit_room_number);

    protected:

    static std::map<int,std::map<int,std::map<int,Room*>>> maze;

};

#endif