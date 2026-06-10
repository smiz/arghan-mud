#ifndef _types_h_
#define _types_h_
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

enum Skill {
    Melee,
    Perception,
    Stealth,
    Swindle,
    Literacy,
    NoSkill
};

Skill to_skill(std::string name);
std::string from_skill(Skill skill);

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
};

using KeyWordList = std::list<std::string>;

/// See short descriptions event flag
#define SEE_SHORT 0x01
/// See long descriptions event flag
#define SEE_LONG 0x02
/// See both kinds of descriptions
#define SEE_BOTH 0x03

#endif