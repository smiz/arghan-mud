#ifndef _model_h_
#define _model_h_
#include "adevs/adevs.h"
#include <map>
#include <set>
#include <list>
#include <cassert>
#include <set>
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
        ENTER_MUD = 1,
        /// @brief Leave all proximity groups and schedule
        LEAVE_MUD,
        /// @brief Join a proximity group
        JOIN_PROX_GROUP,
        /// @brief Leave a proximity group
        LEAVE_PROX_GROUP,
        /// @brief Transfer an item
        TRANSFER_ITEM,
        /// @brief A trap has been triggered!
        TRAP,
        /// @brief Command to drop an item
        DROP_COMMAND,
        /// @brief Command to pick up an item
        GET_COMMAND,
        /// @brief Move in a given direction
        MOVE,
        /// @brief Wandering monsters
        WANDER,
        /// @brief Command to wield a weapon
        WIELD_COMMAND,
        /// @brief Command to stow an equiped item
        STOW_COMMAND,
        /// @brief Hold an item in your secondary hand
        HOLD_COMMAND,
        /// @brief Put an item on
        WEAR_COMMAND,
        /// @brief Look command issued to an actor by a player
        LOOK_COMMAND,
        /// @brief kill command issued to an actor by a player
        KILL_COMMAND,
        /// @brief sneak command issued by a player
        SNEAK_COMMAND,
        /// @brief Warning issued prior to an attack
        PENDING_ATTACK,
        /// @brief Look at something (or being looked at)
        LOOK,
        /// @brief See a description of something
        SEE,
        /// @brief See a description of something after all SEE are processed
        SEE1,
        /// @brief A melee attack
        MELEE_ATTACK,
        /// @brief Result of an attack
        MELEE_RESULT,
        /// @brief Save the model to disk
        SAVE_MODEL,
        /// @brief Practice a skill
        PRACTICE,
        /// @brief, Reroll periodic attributes, like perception
        ROLL_PERIODIC_ATTRIBUTES,
        /// @brief The model was killed, disintegrated, etc
        /// This should stay at the end so that all other
        /// immediate events by the actor (e.g., emitting)
        /// get processed before the actor leaves.
        DESTROYED,
        /// Really the last thing to happen
        RESET_ZONE
    };

    Event():stealthy(0),perceptive(0),flags(0xffff){}
    Event(Type type, int src_id):type(type),src_id(src_id),flags(0xffff){}
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
    /// @brief Item for a transfer or weapon in an attack
    std::shared_ptr<Item> item;
    /// @brief Actor ids that should NOT receive this event
    std::shared_ptr<std::set<int>> exclude;
    /// @brief Was the source sneaking and, if so, how well?
    int stealthy;
    /// @brief How perceptive is the source of the event
    int perceptive;
    /// @brief General purpose flags
    int16_t flags;
    /// @brief Primitive, type specific data
    union {
        /// @brief Prox group to enter or leave
        int prox_group;
        /// @brief Direction of motion for a MOVE event
        Direction dir;
        struct { 
            /// @brief Damage roll (attach) or received (result)
            int dmg_roll;
            /// @brief Attack roll
            int atk_roll;
            /// @brief Was the target killed (result)
            bool killed;
        } melee;
        /// @brief Is the first keyword the name of a container
        struct {
            /// @brief Transfer the item from src_id to dst_id
            bool src_to_dst;
            bool first_keyword_is_container;
        } transfer;
        struct {
            /// @brief Damage inflicted by the trap
            int dmg_roll;
            /// @brief Attribute that affects saving throw
            MonsterAttributes attribute;
            /// @brief Skill that affects saving throw
            Skill skill;
            /// Save target
            int save;
        } trap;
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
// Not part of a zone
#define NO_ZONE -1

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
    /**
     * Should this member be counted as occupying the room for the purposes
     * of calculating zone occupancy. An occupied zone can't be reset.
     */
    virtual bool occupies_space() = 0;

    virtual int hidden() const { return 0; }
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
    ProximityGroup(int group_number = 0, int zone_number = NO_ZONE):
    m_group_number(group_number),m_zone_number(zone_number){
        if (zone_number != NO_ZONE) {
            zone_occupancy[zone_number] = 0;
        }
    }
    /// @brief Add a member to the group when joining
    /// @param member The member to add
    void add_member(ProximityGroupMember* member) {
        if (std::find(m_members.begin(),m_members.end(),member) != m_members.end()) {
            return;
        }
        m_members.push_back(member);
        if (member->occupies_space()) {
            zone_occupancy[m_zone_number] = zone_occupancy[m_zone_number]+1;
        }
        assert(zone_occupancy[m_zone_number] >= 0);
    }
    /// @brief Remove a member when leaving
    /// @param member The member to remove
    void remove_member(ProximityGroupMember* member) {
        if (m_members.remove(member) > 0 && member->occupies_space()) {
            zone_occupancy[m_zone_number] = zone_occupancy[m_zone_number]-1;
        }
        assert(zone_occupancy[m_zone_number] >= 0);
    }
    /// @brief Get the members of the group
    /// @return List of group members.
    const std::list<ProximityGroupMember*>& members() {
        return m_members;
    }
    /// @brief Get the room number (node id) of the group.
    /// @return The node number of the group.
    int group_number() const { return m_group_number; }
    int zone_number() const { return m_zone_number; }
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

    Direction random_exit() const {
        if (exits.empty()) {
            return Direction::EndOfDirectionEnum;
        }
        int pick = rand()%exits.size();
        auto iter = exits.begin();
        for (int i = 0; i < pick; i++) {
            iter++;
        }
        return (*iter).dir;
    }

    /**
     * @brief Is the zone that this group is part of empty
     * @return true if all groups within the zone have no members
     */
    bool zone_is_empty() {
        return zone_occupancy[m_zone_number] == 0;
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

    ProximityGroupMember* find_member(int id) {
        for (auto member: m_members) {
            if (id == member->id()) {
                return member;
            }
        }
        return nullptr;
    }

    private:

    /// @brief Node number of the group
    const int m_group_number;
    /// @brief Which zone is this group part of
    const int m_zone_number;
    /// @brief List of all exits from the room
    std::list<direction_t> exits;
    /// @brief Set of group members
    std::list<ProximityGroupMember*> m_members;

    static std::map<int,int> zone_occupancy;
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
     * @brief Filter on src and dst ids.
     * @return true if it should be discarded
     */
    bool filter(const Event& event);
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
    virtual void hold_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void wear_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void stow_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void kill_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void sneak_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void melee_attack_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void melee_result_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void destroyed_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void pending_attack_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void reset_zone_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void inspect_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void wander_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void trap_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void practice_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void roll_periodic_attributes_event(const Event& event){}

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

    /**
     * @brief Is this model engaged in combat?
     * 
     * The model is engaged in combat if it has
     * any combat related event is scheduled.
     * 
     * @return true if an combat related event is scheduled
     */
    bool in_combat();

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