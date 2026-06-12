#ifndef _trap_h_
#define _trap_h_
#include "model.h"
#include "types.h"

/**
 * Models a trick or trap that is triggered by an
 * occurence and causes a resulting effect, possibly
 * after a delay.
 */
class Trap: public Model {

    public:

    Trap(Graph& graph, std::string file, int group_number);

    protected:

    /// @brief Trap is triggered when someone enters the area
    /// @param event The trap event indicator
    void join_prox_group_event(const Event& event);
    void trap_event(const Event& event);
    /// A trap is part of a room or group and does not occupy it
    bool occupies_space() { return false; }
    Name get_name() const { return name; }
    void reset_zone_event(const Event& event);

    private:
 
    /// @brief File to reload the trap
    std::string file;
    /// @brief Name of the trap
    Name name;
    /// @brief Description of the trap when it fires
    std::string description;
    /// @brief Delay from time triggered to activated
    int delay;
    /// Damage dice for the trace
    Dice dice;
    /// Attribute for saving throw
    MonsterAttributes attr;
    /// Skill for saving throw
    Skill skill;
    /// Save number for the trap
    int save_number;
    /// Event is pending
    bool pending;
};

#endif