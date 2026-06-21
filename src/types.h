#ifndef _types_h_
#define _types_h_
#include "dice.h"
#include <string>
#include <list>
#include <memory>

/**
 * Directions of travel in the mud.
 */
enum Direction {
    North,
    South,
    East,
    West,
    Up,
    Down,
    Flee, // Flee from combat
    EndOfDirectionEnum
};

enum MonsterAttributes {
    Str,
    Dex,
    Con,
    Int,
    Wis,
    Chr,
    NoAttr
};

MonsterAttributes to_monster_attribute(std::string name);
std::string from_monster_attribute(MonsterAttributes attr);

enum Skill {
    Melee,
    Perception,
    Stealth,
    Swindle,
    Literacy,
    Climbing,
    Magic,
    Swimming,
    Steal,
    NoSkill
};

Skill to_skill(std::string name);
std::string from_skill(Skill skill);

enum Effect {
    Refresh,
    ResistPoison,
    Poison,
    NoEffect
};

Effect to_effect(std::string name);
std::string from_effect(Effect effect);

/// @brief Generate or retrieve the 'spell' that creates the effect 
std::string effect_to_word(Effect effect);
/// @brief Generate or retrieve the effect caused by the spell
Effect word_to_effect(std::string word);

/// @brief Slots for equipment that worn
enum WearableSlots {
    Body,
    Neck,
    Unwearable
};

/// @brief A direction of travel from the room
struct direction_t {
    /// @brief To which room does it lead
    int id;
    /// @brief Which direction are we going?
    Direction dir;
    /// @brief What does the exit look like
    std::string description;
    /// @brief Do you need a skill to traverse it?
    Skill skill;
    /// @brief What is the difficulty if it needs a skill?
    int difficulty;
    /// @brief Damage on failure of skill
    std::shared_ptr<Dice> dmg;
    /// @brief Do we go where we want if we fail
    bool go_on_fail;
    /// @brief Message on failure
    std::string fail_msg1;
    std::string fail_msg2;
    /// @brief Message on success
    std::string success_msg1;
    std::string success_msg2;
    /// @brief Is travel in this direction blocked?
    bool blocked;

    direction_t():skill(NoSkill),blocked(false){}
};

using KeyWordList = std::list<std::string>;

/// See short descriptions event flag
#define SEE_SHORT 0x01
/// See long descriptions event flag
#define SEE_LONG 0x02
/// See both kinds of descriptions
#define SEE_BOTH 0x03

#endif