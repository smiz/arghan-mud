#ifndef _model_h_
#define _model_h_
#include "adevs/adevs.h"
#include <map>
#include <set>
#include <list>
#include <cassert>
#include "types.h"
#include "item.h"
#include "name.h"

/**
 * All model states evolve by events.
 * @see Model
 */
struct Event {

    /**
     * Event types have priority just as they are listed.
     * At a tick, lower priority events go first and then
     * higher priorities in order.
     */
    enum Type {
        /// @brief Go to the start area
        ENTER_MUD,
        /// @brief Leave all proximity groups and schedule
        LEAVE_MUD,
        /// @brief Join a proximity group
        JOIN_PROX_GROUP,
        /// @brief Leave a proximity group
        LEAVE_PROX_GROUP,
        /// @brief Transfer an item
        TRANSFER_ITEM,
        /// @brief Command to drop an item
        DROP_COMMAND,
        /// @brief Command to pick up an item
        GET_COMMAND,
        /// @brief Move in a given direction
        MOVE,
        /// @brief Command to wield a weapon
        WIELD_COMMAND,
        /// @brief Command to stow an equiped item
        STOW_COMMAND,
        /// @brief Look command issued to an actor by a player
        LOOK_COMMAND,
        /// @brief Look at something (or being looked at)
        LOOK,
        /// @brief See a description of something
        SEE,
        /// @brief See a description of something after all SEE are processed
        SEE1,
        /// @brief Save the model to disk
        SAVE_MODEL
    };

    Event(){}
    Event(Type type, int src_id):type(type),src_id(src_id){}
    /// @brief The type of the event
    Type type;
    /// @brief Originator of the event
    int src_id;
    /// @brief Destination for the event (can be ANY_ID or ANY_ID_BUT_SRC)
    int dst_id;
    /// @brief Message, if any, associated with the event
    std::string msg;
    /// @brief Pin that this event will be transmitted on
    adevs::pin_t pin;
    /// @brief Any key words associated with a command
    std::shared_ptr<KeyWordList> key_words;
    /// @brief Item for a transfer
    std::shared_ptr<Item> item;
    /// @brief Primitive, type specific data
    union {
        /// @brief Prox group to enter or leave
        int prox_group;
        /// @brief Direction of motion for a MOVE event
        Direction dir;
        /// @brief Transfer the item from src_id to dst_id
        bool transfer_src_to_dst;
    } event_data;
};

using Time = adevs::sd_time<int>;
using Bag = std::list<adevs::PinValue<Event>>;
using Atomic = adevs::Atomic<Event,Time>;
using Graph = adevs::Graph<Event,Time>;
using Simulator = adevs::Simulator<Event,Time>;

// Constant for illegal id. No object will have this id.
#define NO_ID 0
// Const for matching any id
#define ANY_ID -1
// Const for matching any id but the src_id
#define ANY_ID_BUT_SRC -2
// First active proximity group
#define START_GROUP 0

/**
 * Interface that is added to a proximity group when
 * a model joins that group. Proximity groups are used
 * to broadcast events within a Room.
 * @see Room
 * @see Actor
 * @see Model
 * @see ProximityGroup
 */
class ProximityGroupMember {
    public:

    /// @brief Create a group member
    ProximityGroupMember();
    /// @brief Destructor
    virtual ~ProximityGroupMember(){}
    /**
     * @brief Count the matches of keywords.
     * 
     * Return the number of words appearing in the keyword
     * set for this group member.
     * @param words The set of keywords to search for
     * @return Number of matches
     */
    virtual int match_keywords(const KeyWordList& words) const { return 0; }
    /**
     * @brief Get the unique id of the group member. 
     * 
     * This is unique ID used to identify a proximity group
     * when entering or leaving the group.
     * 
     * @param return The unique id of the group member 
     */
    int id() const { return m_id; }
    virtual Name get_name() const { return Name("",true); }

    /// @brief Send an event directly to the member
    const adevs::pin_t pin;

    private:

    /// @brief Next member id. Is incremented in the constructor.
    static int next_id;
    /// @brief id of this member
    const int m_id;
};

/**
 * A proximity group is used to communicate via broadcast
 * within its members. Each Room is mapped to a proximity
 * group, and so the network of groups is the map of the game.
 */
class ProximityGroup {

    public:
    /**
     * @brief Send and receive events on this pin
     * 
     * Connect this pin to a model when it joins the
     * group and disconnect it when it leaves the group.
     * The model will receive its own output and can
     * filter that output on the originator_code of the
     * Event.
     */
    const adevs::pin_t pin;

    /// @brief Create a proximity group with a room id
    /// @param group_number The node number assigned by the Room
    ProximityGroup(int group_number = 0):
    m_group_number(group_number){}
    /// @brief Add a member to the group when joining
    /// @param member The member to add
    void add_member(ProximityGroupMember* member) {
        m_members.push_back(member);
    }
    /// @brief Remove a member when leaving
    /// @param member The member to remove
    void remove_member(ProximityGroupMember* member) {
        m_members.remove(member);
    }
    /// @brief Get the members of the group
    /// @return List of group members.
    const std::list<ProximityGroupMember*>& members() {
        return m_members;
    }
    /// @brief Get the room number (node id) of the group.
    /// @return The node number of the group.
    int group_number() const { return m_group_number; }

    /// @brief Add a direction to exit the group
    /// @param dir Description of the exit direction
    void add_direction(const direction_t& dir) {
        exits.push_back(dir);
    }
    /**
     * Fills the supplied direction with information about
     * dir.dir and returns true. If not found, returns false;
     */
    bool find_direction(direction_t& dir) const {
        for (auto exit_dir: exits) {
            if (exit_dir.dir == dir.dir) {
                dir = exit_dir;
                return true;
            }
        }
        return false;
    }

    /**
     *  @brief Find the id of the member that matches the most keywords
     * 
     *  If no keywords match, it returns the id of the first member.
     *  If rooms always join first, then this returns the room that
     *  models the group. Assumes the group is not empty and that
     *  at least the room it represents is present.
     * 
     * @param key_words The list of keywords to match
     * @return id of the closest match or first member
     */
    int find_best_match(const KeyWordList& key_words) {
        auto iter = m_members.begin();
        int best_id = (*iter)->id();
        int score = (*iter)->match_keywords(key_words);
        iter++;
        for (; iter != m_members.end(); iter++) {
            int new_score = (*iter)->match_keywords(key_words);
            if (new_score > score) {
                best_id = (*iter)->id();
                score = new_score;
            }
        }
        return best_id;
    }
    /// Get id of the first member of the proximity group
    int first_member_id() {
        return m_members.front()->id();
    }

    private:

    /// @brief Node number of the group
    const int m_group_number;
    /// @brief List of all exits from the room
    std::list<direction_t> exits;
    /// @brief Set of group members
    std::list<ProximityGroupMember*> m_members;
};

/**
 * @brief Base class for all active objects in the mud
 * 
 * Always call the event handler of your parent before
 * taking your own actions!
 */
class Model: public Atomic, public ProximityGroupMember {

    public:

    Model(Graph& graph);
    virtual ~Model();

    /** Inherited from adevs. Leave these alone! */
    void delta_int();
    void delta_ext(Time e, const Bag& input);
    void delta_conf(const Bag& input);
    Time ta();
    void output_func(Bag& output);

    protected:

    /**
     * @brief Put an event into this model's schedule
     * 
     * This event will be sent to the proximity group AND
     * be received as input from the proximity group. Use
     * can use the event originator to distinguish these
     * cases. The time to event must be greater than zero.
     * 
     * @param event  The event to schedule
     * @param time_to_event How much time will pass before it occurs.
     */
    void sched_event(Event& event, int time_to_event = 1);

    /**
     * Handler for all Event types. You scheduled it for yourself
     * if the Event source is you. Otherwise, it was sent by
     * somebody else. The handler is triggered when the Event is
     * received.
     */

    /// @brief Default behavior does nothing
    virtual void join_prox_group_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void leave_prox_group_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void see_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void enter_mud_event(const Event& event){};
    /// @brief Default behavior does nothing
    virtual void leave_mud_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void move_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void look_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void look_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void get_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void drop_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void transfer_item_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void save_model_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void wield_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void stow_command_event(const Event& event){}
    
    /// @brief Our proximity group
    ProximityGroup* group;
    /// @brief Items that belong to the model
    std::list<std::shared_ptr<Item>> items;

    /**
     * @brief Find an item by key word
     * 
     * Finds the best match to the key words supplies.
     * Returns nullptr if there is no match.
     * 
     * @param key_words The keys to match
     * @return The best match or nullptr if no match
     */
    std::shared_ptr<Item> find_item(const KeyWordList& key_words);

    /// @brief get the current game time
    /// @return the current time
    int current_time() const { return tNow.real(); }

    /**
     * @brief Causes model to be removed from the game
     * 
     * This method causes the model to be deleted. It
     * must be the very last thing that you call because
     * the object won't exist afterwards. It does not
     * remove you from proximity groups, etc, etc.
     * Cleanup after yourself!
     */
    void leave_game();
    /**
     * @brief Register to receive events on a pin
     * @param pin The pin on which to receive Events
     */
    void receive_from(adevs::pin_t pin);
    /**
     * @brief Stop receiving events on a pin
     * @param pin The pin to unregister
     */
    void do_not_receive_from(adevs::pin_t pin);

    /// @brief Map of room numbers to proximity groups
    static std::map<int,ProximityGroup*> prox_map;

    private:

    /// @brief List of events pending to be sent.
    std::list<std::pair<Time,Event>> pending;
    /// @brief Local time
    Time tNow;
    /// @brief The network of models.
    Graph& graph;
    /// @brief Shared pointer to myself that I add to the graph
    std::shared_ptr<Model> me;
    /// Have we left the game?
    bool departed;
};

#endif