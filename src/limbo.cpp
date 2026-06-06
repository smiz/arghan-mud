#include "limbo.h"

Limbo::Limbo(Graph& graph):
Room(graph,"limbo",Room::next_free_room_number()) {
    
}