#ifndef _actor_h_
#define _actor_h_
#include "model.h"
#include "name.h"
#include "item.h"

struct initial_stats_t {
    int str, dex, con, intel, wis, chr, hp;
};

/**
 * Models a creature. These should be 
 * instantiated after the rooms so that room
 * events always happen before actor events.
 */
class Actor: public Model {

    public:

    /**
     * @brief Create an actor from a YAML file.
     * 
     * If fd = -1 this actor will not produce text.
     * Otherwise, messages are written to the file
     * descriptor.
     * 
     * @param graph The world that the actor belongs in
     * @param filename Where the actor data is stored
     * @param load If true, load from the file. Write to it is false.
     * @param name Name of the new actor
     * @param pc Is this a player character?
     */
    Actor(Graph& graph,
        std::string filename,
        bool load,
        std::string name = "",
        bool pc = false,
        const initial_stats_t* const initial_status = nullptr,
        int regen_room_number = 0);

    Name get_name() const { return name; }
    void set_msg_fd(int fd) { this->fd = fd; }
    int match_keywords(const KeyWordList& key_words) const;
    void report_inventory();
    void report_stats();
    void report_skills();

    void toggle_short_descriptions() { short_descriptions = !short_descriptions; }
    void save();

    static initial_stats_t initial_stats();

    void set_password(std::string passwd) { password = passwd; }
    const std::string& get_password() const { return password; }
    /// @brief Write a message to the console if fd is valid
    /// @param msg The message to write 
    void message(std::string msg);
    
    int get_fd() const { return fd; }

    /// Actors are counted toward zone occupancy
    bool occupies_space() { return true; }
    int hidden() const { return sneaking; }
    protected:

    std::string detail;
    std::string description;
    Name name;
    std::string file;
    KeyWordList key_words;

    /// Natural, unmodified str
    int strength;
    /// Natural, unmodified dex
    int dexterity;
    /// Natural, unmodified con
    int constitution;
    /// Natural, unmodified int
    int intelligence;
    /// Natural, unmodified wis
    int wisdom;
    /// Natural, unmodified chr
    int charisma;
    /// Natural, unmodified hp (unwounded)
    int hit_points;
    /// Damage sustained
    int damage;
    /// Natural, unmodified ac
    int armor_class;

    /// Equipment slot for the primary hand (weapon hand)
    std::shared_ptr<Item> primary_hand;
    /// Equipment slot for the secondary hand (shield hand)
    std::shared_ptr<Item> secondary_hand;
    /// Equipment slot for an object that is worn on the body
    std::shared_ptr<Item> body;
    /// Equipment slot for something work around the neck
    std::shared_ptr<Item> neck;
    /**
     * @brief Emit a message to the proximity group or an individual
     * 
     * @param msg The message to send
     * @param dst_id The target of the message
     */
    void emit(std::string msg, int dst_id = ANY_ID_BUT_SRC, int exclude = -1);
    void emit_stealthy(std::string msg, int dst_id = ANY_ID_BUT_SRC, int exclude = -1);

    void stow_command_event(const Event& event);
    void wield_command_event(const Event& event);
    void hold_command_event(const Event& event);
    void wear_command_event(const Event& event);
    void look_command_event(const Event& event);
    void get_command_event(const Event& event);
    void drop_command_event(const Event& event);
    void join_prox_group_event(const Event& event);
    void leave_prox_group_event(const Event& event);
    void enter_mud_event(const Event& event);
    void leave_mud_event(const Event& event);
    void see_event(const Event& event);
    void quit_mud_event(const Event& event);
    void move_event(const Event& event);
    void look_event(const Event& event);
    void transfer_item_event(const Event& event);
    void save_model_event(const Event& event);
    void kill_command_event(const Event& event);
    void melee_attack_event(const Event& event);
    void melee_result_event(const Event& event);
    void pending_attack_event(const Event& event);
    void destroyed_event(const Event& event);
    void wander_event(const Event& event);
    void trap_event(const Event& event);
    void practice_event(const Event& event);
    void roll_periodic_attributes_event(const Event& event);
    void sneak_command_event(const Event& event);
    void hear_event(const Event& event);
    void schedule_destroyed();
    void schedule_attack(int target_id, bool warn = true);
    void swindle_command_event(const Event& event);
    void swindle_event(const Event& event);
    void start_swindle_event(const Event& event);
    void swindle_result_event(const Event& event);

    /**
     * Return a skill roll for using an item. Returns
     * -1 if the item cannot be used.
     */
    int use_item(std::shared_ptr<Item>& item);

    /**
     * This is how long it takes for the actor to make
     * an attack.
     */
    int melee_attack_delay(std::shared_ptr<Item>& item);

    int use_skill(Skill skill, bool average = false);

    /** 
     * Calculate the ac of the actor. This is natural ac
     * plus equipment and other bonuses.
     */
    int total_ac();

    static int attribute_modifier(int attribute_score);

    /// Table containing all of the actor's skills
    std::map<Skill,int> skills;

    /// List of entities that are hated by the actor and
    /// will be attacked on site
    std::set<int> hates;
    /// @brief This monster will wander throughout its zone
    /// Monster wanders if > 0.
    int wanders;
    /// Monster is aggressive if true
    bool aggressive;

    void wander();

    std::pair<std::string,std::string> damage_adjective();

    int sneaking, perceiving;
    /// @brief Set of names that I will assist if they are attacked
    std::set<std::string> assist;

    void account_for_cost(const std::shared_ptr<Item>& item, bool selling);
    int consolidate_coins();

    std::vector<std::string> rumors;

    private:

    void change_prox_groups(int new_group);
    void init(const initial_stats_t* const stats);
    void sched_save();

    void gain_xp(int xp);
 

    // Character level
    int level;
    // XP for next level
    int xp_to_go;
    // Free skill slots
    int free_skill_slots;

    int fd;
    /// @brief Node at which the actor left the mud
    int exit_node_id;
    /// @brief Node at which we always regenerate after death
    const int regen_node_number;
    /// @brief Is that actor pc that should be saved?
    const bool pc;

    std::string password;

    bool short_descriptions;
};

#endif