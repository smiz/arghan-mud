#include "types.h"

MonsterAttributes to_monster_attribute(std::string name) {
    if (name == "strength" || name == "str") {
        return Str;
    } else if (name == "dexterity" || name == "dex") {
        return Dex;
    } else if (name == "constitution" || name == "con") {
        return Con;
    } else if (name == "intelligence" || name == "int") {
        return Int;
    } else if (name == "wisdom" || name == "wis") {
        return Wis;
    } else if (name == "charisma" || name == "chr") {
        return Chr;
    }
    throw "bad attributed name "+name;
    return NoAttr;
}

Skill to_skill(std::string name) {
    if (name == "melee") {
        return Melee;
    } else if (name == "perception") {
        return Perception;
    } else if (name == "stealth") {
        return Stealth;
    } else if (name == "swindle") {
        return Swindle;
    } else if (name == "literacy") {
        return Literacy;
    } else if (name == "climbing") {
        return Climbing;
    }
    throw "bad skill name "+name;
    return NoSkill;
}

std::string from_skill(Skill skill) {
    if (skill == Melee) {
        return "melee";
    } else if (skill == Perception) {
        return "perception";
    } else if (skill == Stealth) {
        return "stealth";
    } else if (skill == Swindle) {
        return "swindle";
    } else if (skill == Literacy) {
        return "literacy";
    } else if (skill == Climbing) {
        return "climbing";
    }
    return "";
}