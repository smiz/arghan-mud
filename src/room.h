#ifndef _room_h_
#define _room_h_
#include "model.h"
#include "item.h"

/**
 * Models a location. These should be instantiated
 * first when the mud is loaded to give them the 
 * smallest logical field in the event scheduler.
 * Events occurring when you enter or leave a room,
 * such as its description, should happen before
 * any other events, such as being attacked by a
 * mob.
 */
class Room: public Model {

    public:

    Room(Graph& graph, std::string file);

    protected:

    std::string description;
    void join_prox_group_event(const Event& event);
    void look_event(const Event& event);
    void transfer_item_event(const Event& event);

    private:
    
    void sched_see_event(int src_id, const KeyWordList& key_words);
};

#endif