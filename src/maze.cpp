#include "maze.h"

std::map<int,std::map<int,std::map<int,Room*>>> Maze::maze;

static std::string room_description() {
    int pick = rand()%4;
    switch(pick) {
        case 0: return "maze/room";
        case 1: return "maze/room_a";
        case 2: return "maze/room_b";
    }
    return "maze/tunnel";
}

#define NUM_ROOMS 20
#define EXTENT 20

Maze::Maze(Graph& graph, int exit_room_number):
Room(graph,room_description(),MAZE_START_ROOM_NUMBER) {
    int room_number = MAZE_START_ROOM_NUMBER+1;
    int num_rooms = 0;
    Room* origin = this;
    direction_t direction;
    direction.dir = Up;
    direction.id = exit_room_number;
    direction.description = "A staircase climbs into sunlight.";
    group->add_direction(direction);
    int x = 0, y = 0, z = 0;
    maze[x][y][z] = this;
    while (num_rooms < NUM_ROOMS) {
        int xx = x, yy = y, zz = z;
        int dir = rand()%4;
        switch(dir) {
            case 0:
                xx++;
                direction.dir = East;
                direction.description = "A tunnel goes east.";
                break;
            case 1:
                xx--;
                direction.dir = West;
                direction.description = "A tunnel goes west.";
                break;
            case 2:
                yy++;
                direction.dir = North;
                direction.description = "A tunnel goes north.";
                break;
            case 3:
                yy--;
                direction.dir = South;
                direction.description = "a tunnel goes south.";
                break;
        }
        if (abs(xx) > EXTENT || abs(yy) > EXTENT) {
            continue;
        }
        Room* room = nullptr;
        if (maze.find(xx) != maze.end()) {
            if (maze[xx].find(yy) != maze[xx].end()) {
                if (maze[xx][yy].find(zz) != maze[xx][yy].end()) {
                    room = maze[xx][yy][zz];
                }
            }
        }
        if (room == nullptr) {
            room = new Room(graph,room_description(),room_number++);
            num_rooms++;
            maze[xx][yy][zz] = room;
        }
        if (!origin->get_group()->find_direction(direction)) {
            direction.id = room->get_group()->group_number();
            origin->get_group()->add_direction(direction);
        }
        if (direction.dir == South) {
            direction.dir = North;
            direction.description = "A tunnel goes north.";
        } else if (direction.dir == North) {
            direction.dir = South;
            direction.description = "a tunnel goes south.";
        } else if (direction.dir == East) {
            direction.dir = West;
            direction.description = "A tunnel goes west.";
        } else { // West
            direction.dir = East;
            direction.description = "A tunnel goes east.";
        }
        if (!room->get_group()->find_direction(direction)) {
            direction.id = origin->get_group()->group_number();
            room->get_group()->add_direction(direction);
        }
        x = xx;
        y = yy;
        z = zz;
        origin = room;
    }
}