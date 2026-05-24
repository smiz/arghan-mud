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
    }
    throw "bad skill name "+name;
    return NoSkill;
}

std::string from_skill(Skill skill) {
    if (skill == Melee) {
        return "melee";
    }
    return "";
}