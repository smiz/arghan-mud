
#ifndef _limbo_h_
#define _limbo_h_
#include "room.h"

/**
 * A unique starting room for each character.
 */
class Limbo: public Room {

    public:

    Limbo(Graph& graph);
};

#endif