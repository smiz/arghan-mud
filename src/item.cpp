#include "item.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>

const std::string Item::items_directory = "items/";

Item::Item():
xp(0),
m_held_ac_bonus(0),
m_worn_ac_bonus(0),
m_cost(0),
m_lock(0),
m_key(0),
locked(false),
heavy(false),
m_message_complexity(0),
m_wearable(WearableSlots::Unwearable),
m_modifier(MonsterAttributes::NoAttr),
m_skill(Skill::NoSkill),
key_words(std::make_shared<KeyWordList>()) {
    use_effect.type = NoEffect;
    use_effect.difficulty = 0;
}

Item::Item(std::string file):
xp(0),
m_held_ac_bonus(0),
m_worn_ac_bonus(0),
m_cost(0),
m_lock(0),
m_key(0),
locked(false),
heavy(false),
m_message_complexity(0),
m_wearable(WearableSlots::Unwearable),
m_modifier(MonsterAttributes::NoAttr),
m_skill(Skill::NoSkill),
key_words(std::make_shared<KeyWordList>()),
m_filename(file) {
    use_effect.type = NoEffect;
    use_effect.difficulty = 0;
    YAML::Node yaml = YAML::LoadFile(items_directory+file.c_str());
    this->m_name = Name(yaml["name"].as<std::string>(),yaml["proper_name"].as<bool>());
    m_description = yaml["description"].as<std::string>();
    m_detail = yaml["detail"].as<std::string>();
    const YAML::Node& keyword_list = yaml["keywords"];
    for (const auto& word : keyword_list) {
        key_words->push_back(word.as<std::string>());
    }
    if (yaml["xp"]) {
        xp = yaml["xp"].as<int>();
    }
    if (yaml["message"]) {
        m_message = yaml["message"].as<std::string>();
        if (yaml["message_complexity"]) {
            m_message_complexity = yaml["message_complexity"].as<int>();
        }
    }
    if (yaml["weapon_data"]) {
        m_weapon_info.speed = yaml["weapon_data"]["speed"].as<int>();
        m_weapon_info.dmg_die = Dice(yaml["weapon_data"]["damage"].as<std::string>());
        m_held_ac_bonus = yaml["weapon_data"]["ac"].as<int>();
    }
    if (yaml["attribute"]) {
        std::string attr = yaml["attribute"].as<std::string>();
        m_modifier = to_monster_attribute(attr);
    }
    if (yaml["skill"]) {
        m_skill = to_skill(yaml["skill"].as<std::string>());
    }
    if (yaml["wearable"]) {
        std::string location = yaml["wearable"]["location"].as<std::string>();
        if (location == "body") {
            m_wearable = WearableSlots::Body;
        } else if (location == "neck") {
            m_wearable = WearableSlots::Neck;
        } else {
            throw file+" has bad wearable entry";
        }
        if (yaml["wearable"]["ac"]) {
            m_worn_ac_bonus = yaml["wearable"]["ac"].as<int>();
        }
    }
    if (yaml["contents"]) {
        m_contents = std::make_shared<std::list<std::shared_ptr<Item>>>();
        const YAML::Node& item_list = yaml["contents"];
        for (const auto& item : item_list) {
            auto new_item = std::make_shared<Item>(item.as<std::string>());
            m_contents->push_back(new_item);
        }
    }
    if (yaml["lock"]) {
        m_lock = yaml["lock"].as<int>();
        locked = true;
    }
    if (yaml["key"]) {
        m_key = yaml["key"].as<int>();
    }
    if (yaml["cost"]) {
        m_cost = yaml["cost"].as<int>();
    }
    if (yaml["heavy"]) {
        heavy = yaml["heavy"].as<bool>();
    }
    if (yaml["effect"]) {
        use_effect.type = to_effect(yaml["effect"]["name"].as<std::string>());
        use_effect.intensity = yaml["effect"]["intensity"].as<int>();
        use_effect.consumed = yaml["effect"]["consumed"].as<bool>();
        use_effect.verb = yaml["effect"]["verb"].as<std::string>();
        use_effect.word = false;
        if (yaml["effect"]["difficulty"]) {
            use_effect.difficulty = yaml["effect"]["difficulty"].as<int>(); 
        }
        if (yaml["effect"]["word_of_power"]) {
            use_effect.word = yaml["effect"]["word_of_power"].as<bool>();
            m_message2 = " The word is " + effect_to_word(use_effect.type);
        }

    }
}

void Item::save() {
    YAML::Node yaml;
    yaml["name"] = m_name.get_name();
    yaml["proper_name"] = m_name.is_proper();
    yaml["description"] = m_description;
    yaml["detail"] = m_detail;
    yaml["keywords"] = *key_words;
    if (xp > 0) {
        yaml["xp"] = xp;
    }
    if (m_message.length() > 0) {
        yaml["message"] = m_message;
        yaml["message_complexity"] = m_message_complexity;
    }
    yaml["weapon_data"]["speed"] = m_weapon_info.speed;
    yaml["weapon_data"]["damage"] = m_weapon_info.dmg_die.str();
    yaml["weapon_data"]["ac"] = m_held_ac_bonus; 
    if (m_modifier != NoAttr) {
        yaml["attribute"] = from_monster_attribute(m_modifier);
    }
    if (m_skill != NoSkill) {
        yaml["skill"] = from_skill(m_skill);
    }
    if (m_wearable != Unwearable) {
        if (m_wearable == WearableSlots::Body) {
            yaml["wearable"]["location"] = "body";
        } else if (m_wearable == WearableSlots::Neck) {
            yaml["wearable"]["location"] = "neck";
        }
        yaml["wearable"]["ac"] = m_worn_ac_bonus;
    }
    if (m_contents != nullptr) {
        std::list<std::string> empty_list;
        yaml["contents"] = empty_list;
    }
    yaml["cost"] = m_cost;
    if (m_key != 0) {
        yaml["key"] = m_key;
    }
    if (use_effect.type != NoEffect) {
        yaml["effect"]["name"] = from_effect(use_effect.type);
        yaml["effect"]["intensity"] = use_effect.intensity;
        yaml["effect"]["consumed"] = use_effect.consumed;
        yaml["effect"]["verb"] = use_effect.verb;
        yaml["effect"]["difficulty"] = use_effect.difficulty;
        yaml["effect"]["word_of_power"] = use_effect.word;
    }
    std::ofstream fout(m_filename.c_str());
    fout << yaml;
    fout.close();
}

std::shared_ptr<Item> Item::remove_item(const KeyWordList& key_words) {
    int best_score = 0;
    std::shared_ptr<Item> pick;
    auto iter = m_contents->begin();
    for (; iter != m_contents->end(); iter++) {
        int score = (*iter)->match_keywords(key_words);
        if (score > best_score) {
            best_score = score;
            pick = *iter;
        }
    }
    if (pick != nullptr) {
        m_contents->remove(pick);
    }
    return pick;
}

int Item::match_keywords(const KeyWordList& key_words) const {
    int score = 0;
    for (auto word: *(this->key_words)) {
        if (std::find(key_words.begin(),key_words.end(),word) != key_words.end()) {
            score++;
        }
    }
    return score;
}

int Item::get_cost() const {
    int sum = m_cost;
    if (m_contents != nullptr) {
        for (auto iter = m_contents->begin(); iter != m_contents->end(); iter++) {
            sum += (*iter)->get_cost();
        }
    }
    return sum;
}