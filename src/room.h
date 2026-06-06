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

    /**
     * Create a room from the supplied description file.
     * If node_id != -1 then the supplied room id overrides
     * the description id. This feature is for making
     * areas at random and isn't normally used.
     */
    Room(Graph& graph, std::string file, int node_id = -1);

    ProximityGroup* get_group() { return group; }

    protected:

    std::string description;
    void join_prox_group_event(const Event& event);
    void look_event(const Event& event);
    void transfer_item_event(const Event& event);
    void reset_zone_event(const Event& event);
    void destroyed_event(const Event& event);
    void lock_unlock_event(const Event& event);

    /// A room does not occupy itself!
    bool occupies_space() { return false; }

    static int next_free_room_number();

    private:
 
    /// @brief File to reload the room
    std::string file;
    /// @brief Graph for reloading monsters
    Graph& graph;
    /// @brief Is this the initial load?
    bool initial_load;

    static int max_room_number;

    std::string short_description;

    void sched_see_event(int src_id, const KeyWordList& key_words, bool first_word_is_container, int16_t flags = SEE_BOTH);
    void reload();

};

#endif