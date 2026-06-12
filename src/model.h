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
 * @brief Data structure used to change model states.
 * 
 * All model states evolve by events. This data structure
 * encodes all of the information needed to calculate
 * a change of state in response to an event.
 * 
 * The use of fields in the event are idiomatic to
 * the type of event. You should study the event
 * handler provided by the Actor, Room, or other
 * Model subclass to understand what is going on.
 * 
 * @see Model
 */
struct Event {

    /**
     * @brief List of all types of events in the mud
     * 
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
        /// @brief Transfer an item to/from a Room
        TRANSFER_ITEM,
        /// @brief A trap has been triggered!
        TRAP,
        /// @brief Command to drop an item
        DROP_COMMAND,
        /// @brief Command to pick up an item
        GET_COMMAND,
        /// @brief Move in a given direction
        MOVE,
        /// @brief Wandering monsters wander
        WANDER,
        /// @brief Command to wield a weapon
        WIELD_COMMAND,
        /// @brief Command to stow an equipped item
        STOW_COMMAND,
        /// @brief Hold an item in your secondary hand
        HOLD_COMMAND,
        /// @brief Put an item on
        WEAR_COMMAND,
        /// @brief Look command issued by a player
        LOOK_COMMAND,
        /// @brief Kill command issued by a player
        KILL_COMMAND,
        /// @brief Sneak command issued by a player
        SNEAK_COMMAND,
        /// @brief Swindle command
        SWINDLE_COMMAND,
        /// @brief Start of a swindle
        START_SWINDLE,
        /// @brief Do the swindle
        SWINDLE,
        /// @brief Result of a swindle
        SWINDLE_RESULT, 
        /// @brief Lock or unlock command
        LOCK_UNLOCK_COMMAND,
        /// @brief Do the lock or unlock of something
        LOCK_UNLOCK,
        /// @brief Warning issued prior to an attack
        PENDING_ATTACK,
        /// @brief Look at something (or being looked at)
        LOOK,
        /// @brief See a description of something
        SEE,
        /// @brief A melee attack
        MELEE_ATTACK,
        /// @brief Result of an attack
        MELEE_RESULT,
        /// @brief See a description of something after all SEE are processed
        SEE1,
        /// @brief Hear something
        HEAR,
        /// @brief Save the model to disk
        SAVE_MODEL,
        /// @brief Practice a skill
        PRACTICE,
        /// @brief Read something
        READ,
        /// @brief Used for periodic actions, like healing
        ROLL_PERIODIC_ATTRIBUTES,
        /// @brief The model was killed, disintegrated, etc.
        DESTROYED,
        /// Really the last thing to happen
        RESET_ZONE
    };

    /**
     * @brief Create an uninitialized event
     * 
     * The stealthy, perceptive, and flags fields are initialized.
     * The flags are 0xffff. Others are zero.
     */
    Event():stealthy(0),perceptive(0),flags(0xffff){}
    /**
     * @brief Create an event.
     *
     * Initializes as the default constructor and sets the src_id and
     * type variables.
     *  
     * @param type of the event
     * @param src_id Source id of the event.
     */
    Event(Type type, int src_id):type(type),src_id(src_id),stealthy(0),perceptive(0),flags(0xffff){}
    /// @brief The type of the event
    Type type;
    /// @brief Originator of the event
    int src_id;
    /// @brief Destination for the event (can also be ANY_ID or ANY_ID_BUT_SRC)
    int dst_id;
    /// @brief Message, if any, associated with the event
    std::string msg;
    /// @brief Pin that this event will be transmitted on
    adevs::pin_t pin;
    /// @brief Any key words associated with a command
    std::shared_ptr<KeyWordList> key_words;
    /// @brief Second list of key words associated with a command
    std::shared_ptr<KeyWordList> key_words2;
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
        /// @brief Who is the subject of a SEE or SEE1 event?
        int subject_id;
        /// @brief What is the swindling skill roll?
        int swindling;
        /// @brief What is the literacy skill roll?
        int literacy;
        /// @brief Prox group to enter or leave
        int prox_group;
        /// @brief Lock or unlock an object?
        bool lock;
        /// @brief Direction of motion for a MOVE event
        Direction dir;
        /// @brief Data for MELEE events
        struct { 
            /// @brief Damage roll caused (ATTACK) or received (RESULT)
            int dmg_roll;
            /// @brief Attack roll
            int atk_roll;
            /// @brief Was the target killed (RESULT)
            bool killed;
        } melee;
        /// @brief Data for TRANSFER events
        struct {
            /// @brief Transfer the item from src_id to dst_id
            bool src_to_dst;
            /// @brief Is the first keyword in the list a container name?
            bool first_keyword_is_container;
            /// @brief How many coins are possessed by the actor attempting the transfer
            int coins_in_purse;
        } transfer;
        /// @brief Data for TRAP events
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

/// Constant for illegal id. No object will have this id.
#define NO_ID 0
/// Const for matching any id
#define ANY_ID -1
/// Const for matching any id but the src_id
#define ANY_ID_BUT_SRC -2
/// Not part of a zone
#define NO_ZONE -1

/**
 * @brief A member of a ProximityGroup
 * 
 * Interface that is added to a ProximityGroup when
 * a model joins that group. A ProximityGroup is used
 * to exchange events within a Room.
 * 
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
     * 
     * @param words The set of keywords to search for
     * @return Number of matches
     */
    virtual int match_keywords(const KeyWordList& words) const { return 0; }
    /**
     * @brief Get the unique id of the group member. 
     * 
     * This is a unique id used to identify a proximity group
     * member in the mud. It is globally unique.
     * 
     * @return The globally unique id of the group member 
     */
    int id() const { return m_id; }
    /**
     * @brief Get the Name of the group member
     */
    virtual Name get_name() const { return Name("",true); }
    /**
     * @brief Does the member inhibit resets of a zone?
     * 
     * Should this member be counted as occupying the room for the purposes
     * of calculating zone occupancy. An occupied zone can't be reset.
     */
    virtual bool occupies_space() = 0;

    /**
     * @brief How well is the member hidden?
     * 
     * This is used to confirm information about sneaking or otherwise
     * hard see see members.
     * 
     * @return The difficulty of detecting the member.
     */
    virtual int hidden() const { return 0; }
    /// @brief Pin for sending an event directly to the member
    const adevs::pin_t pin;

    private:

    /// @brief Next member id. Is incremented in the constructor.
    static int next_id;
    /// @brief id of this member
    const int m_id;
};

/**
 * @brief Groups together ProximityGroupMembers to model a physical space.
 * 
 * A proximity group is used to communicate via broadcast
 * within its members. Each Room is mapped to a proximity
 * group, and so the network of groups is the map of the game.
 */
class ProximityGroup {

    public:
    /**
     * @brief Send and receive events to the group using this pin
     * 
     * Connect this pin to a model when it joins the
     * group and disconnect it when it leaves the group.
     * The model will receive its own output and can
     * filter that output on the originator_code of the
     * Event.
     */
    const adevs::pin_t pin;

    /**
     * @brief Create a proximity group with a room id
     * 
     * Each room has a group number and zone number. Group numbers
     * must be globally unique and are assigned when the room is
     * created. The zone number is used to control wandering Actor
     * objects. All groups with the same zone number reset at
     * the same time. if NO_ZONE is used, the group will never
     * reset.
     * 
     * @param group_number The node number assigned by the Room.
     * @param zone_number Zone to which the group belongs. Can be NO_ZONE.
     * @param shop Is this a shop? If so, picking up and dropping items involves Coin.
     */
    ProximityGroup(int group_number = 0, int zone_number = NO_ZONE, bool shop = false):
    m_group_number(group_number),m_zone_number(zone_number),m_shop(shop){
        if (zone_number != NO_ZONE) {
            zone_occupancy[zone_number] = 0;
        }
    }
    /// @brief Add a member to the group when joining
    /// @param member The member to add
    void add_member(ProximityGroupMember* member) {
        /// Avoid duplicates! Maybe we should use a set instead of checking
        /// for membership.
        if (std::find(m_members.begin(),m_members.end(),member) != m_members.end()) {
            return;
        }
        m_members.push_back(member);
        /// Does this occupant inhibit resets?
        if (member->occupies_space()) {
            zone_occupancy[m_zone_number] = zone_occupancy[m_zone_number]+1;
        }
        assert(zone_occupancy[m_zone_number] >= 0);
    }
    /// @brief Remove a member when leaving
    /// @param member The member to remove
    void remove_member(ProximityGroupMember* member) {
        /// Does this occupant inhibit resets?
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
    /// @brief Get the zone number for the group.
    /// @return The zone to which the group belongs.
    int zone_number() const { return m_zone_number; }
    /// @brief Add a direction to exit the group
    /// @param dir Description of the exit direction
    void add_direction(const direction_t& dir) {
        exits.push_back(dir);
    }
    /**
     * @brief get information about an exit
     * 
     * Fills the supplied direction with information about
     * dir.dir and returns true. If not found, returns false;
     * 
     * @param dir Reads dir.dir. If in the exist list, the
     * other fields are set to match the stored exist.
     * @return True if the direction exists, false otherwise.
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
     * @brief Select an exit at random.
     * 
     * Returns a Direction of travel. Used for fleeing
     * from combat and for wandering Actor objects.
     * 
     * @return A valid direction or Direction::EndOfDirectionEnum
     * if no valid exit exists.
     */
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
     * @return true if all groups within the zone have no occupants
     * 
     * @see ProximityGroupMember
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
    /// @brief Get id of the first member of the proximity group
    int first_member_id() {
        return m_members.front()->id();
    }

    /**
     * @brief Find a member with the given id.
     * 
     * Look for a member having the supplied id. Returns
     * the member if it is found.
     * 
     * @param id the member id to look for
     * @return the member or nullptr if not found.
     */
    ProximityGroupMember* find_member(int id) {
        for (auto member: m_members) {
            if (id == member->id()) {
                return member;
            }
        }
        return nullptr;
    }

    /// @brief Is this group a shop?
    bool is_shop() const { return m_shop; }
    /**
     * @brief Set the shop flag.
     * 
     * This is set when the group is loaded.
     * 
     * @see Room
     * @param shop true if this room is a shop, false if not
     */ 
    void set_shop(bool shop) { m_shop = shop; }

    private:

    /// @brief Node number of the group
    const int m_group_number;
    /// @brief Which zone is this group part of
    const int m_zone_number;
    /// @brief List of all exits from the room
    std::list<direction_t> exits;
    /// @brief Set of group members
    std::list<ProximityGroupMember*> m_members;
    /// @brief Is this zone a shop?
    bool m_shop;

    /// @brief Globally visible map of zone occupancy counts
    static std::map<int,int> zone_occupancy;
};

/**
 * @brief Base class for all active objects in the mud
 * 
 * Always call the event handler of your parent before
 * taking your own actions! Or at least be certain your
 * parent doesn't do anything.
 * 
 * @see Actor
 * @see Room
 * @see Trap
 */
class Model: public Atomic, public ProximityGroupMember {

    public:

    /**
     * @brief Create an uninitialized model
     * 
     * Create a model and add it to the adevs::Graph.
     * 
     * @param graph This is the adevs::Graph that contains all Model
     * objects and objects derived from Model.
     */
    Model(Graph& graph);
    /// @brief Destructor
    virtual ~Model();

    /// @brief Inherited from adevs
    void delta_int();
    /// @brief Inherited from adevs
    void delta_ext(Time e, const Bag& input);
    /// @brief Inherited from adevs
    void delta_conf(const Bag& input);
    /// @brief Inherited from adevs
    Time ta();
    /// @brief Inherited from adevs
    void output_func(Bag& output);

    protected:

    /**
     * @brief Filter on src and dst ids.
     *
     * In practice, the filter is only useful for
     * Actor and Trap objects. It isn't used by Room
     * objects. Really, this is a bug and not a 
     * feature. Someone should fix the filter().
     * 
     * @see Actor
     * @see Trap
     * @return true if the should be discarded
     */
    bool filter(const Event& event);
    /**
     * @brief Put an event into this model's schedule
     * 
     * This event will be sent to the proximity group AND
     * be received as input from the proximity group. You
     * can use the event originator to distinguish these
     * cases. The time to event must be greater than zero.
     * 
     * @param event  The event to schedule
     * @param time_to_event How much time will pass before it occurs.
     */
    void sched_event(Event& event, int time_to_event = 1);

    /**
     * @brief Cancel a scheduled event
     * 
     * Cancels all of the Model scheduled events of the given type.
     * 
     * @param type The type of event to cancel
     */
    void cancel_event(Event::Type type);

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
    /// @brief Default behavior does nothing
    virtual void hear_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void swindle_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void swindle_result_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void swindle_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void start_swindle_event(const Event& event);
    /// @brief Default behavior does nothing
    virtual void lock_unlock_command_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void lock_unlock_event(const Event& event){}
    /// @brief Default behavior does nothing
    virtual void read_event(const Event& event){}
    /// @brief Our proximity group
    ProximityGroup* group;
    /**
     * @brief Items that belong to the model
     * 
     * For a Room, these are the contents of the space.
     * For an Actor, this is just the inventory not being
     * held, worn, or carried in a container.
     */
    std::list<std::shared_ptr<Item>> items;

    /**
     * @brief Find an item by key word
     * 
     * Finds the best match to the key words supplies.
     * Returns nullptr if there is no match. Looks in
     * the list of items for this Model.
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

    /**
     * @brief Map of room numbers to proximity groups
     * 
     * This stores the MUD map! This lets you find
     * ProximityGroup objects by number. Then the group
     * tells you which groups are reachable from it
     * and in what direction.
     * 
     * @see ProximityGroup
     */
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