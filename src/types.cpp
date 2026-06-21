#include "types.h"
#include <random>

static std::list<std::pair<Effect,std::string>> words_of_power;
static std::random_device rd;
static std::uniform_int_distribution rand_char(97,122);
static std::mt19937 gen(rd());

std::string effect_to_word(Effect effect) {
    for (auto wop: words_of_power) {
        if (wop.first == effect) {
            return wop.second;
        }
    }
    std::string word;
    word.push_back(rand_char(gen));
    word.push_back(rand_char(gen));
    word.push_back(rand_char(gen));
    word.push_back(rand_char(gen));
    words_of_power.push_back(std::pair<Effect,std::string>(effect,word));
    return word;
}

Effect word_to_effect(std::string word) {
    for (auto wop: words_of_power) {
        if (wop.second == word) {
            return wop.first;
        }
    }
    return NoEffect;
}

std::string from_monster_attribute(MonsterAttributes attr) {
    if (attr == Str) {
        return "str";
    } else if (attr == Dex) {
        return "dex";
    } else if (attr == Con) {
        return "con";
    } else if (attr == Int) {
        return "int";
    } else if (attr == Wis) {
        return "wis";
    } else {
        return "chr";
    }
}

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
    } else if (name == "magic") {
        return Magic;
    } else if (name == "swimming") {
        return Swimming;
    } else if (name == "steal") {
        return Steal;
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
    } else if (skill == Swimming) {
        return "swimming";
    } else if (skill == Magic) {
        return "magic";
    } else if (skill == Steal) {
        return "steal";
    }
    return "";
}

Effect to_effect(std::string name) {
    if (name == "refresh") {
        return Refresh;
    } else if (name == "resist poison") {
        return ResistPoison;
    } else if (name == "poison") {
        return Poison;
    }
    throw "bad effect name "+name;
    return NoEffect;
}

std::string from_effect(Effect effect) {
    if (effect == Refresh) {
        return "refresh";
    } else if (effect == ResistPoison) {
        return "resist poison";
    } else if (effect == Poison) {
        return "poison";
    }
    return "";
}