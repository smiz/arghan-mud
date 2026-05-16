#include "item.h"
#include <yaml-cpp/yaml.h>
#include <iostream>

const std::string Item::items_directory = "items/";

Item::Item(std::string file):
m_held_ac_bonus(0),
m_worn_ac_bonus(0),
m_wearable(WearableSlots::Unwearable),
m_modifier(MonsterAttributes::NoAttr),
key_words(std::make_shared<KeyWordList>()),
m_filename(file) {
    YAML::Node yaml = YAML::LoadFile(items_directory+file.c_str());
    this->m_name = Name(yaml["name"].as<std::string>(),yaml["proper_name"].as<bool>());
    m_description = yaml["description"].as<std::string>();
    m_detail = yaml["detail"].as<std::string>();
    const YAML::Node& keyword_list = yaml["keywords"];
    for (const auto& word : keyword_list) {
        key_words->push_back(word.as<std::string>());
    }
    if (yaml["weapon_data"]) {
        m_weapon_info.speed = yaml["weapon_data"]["speed"].as<int>();
        m_weapon_info.dmg_die = Dice(yaml["weapon_data"]["damage"].as<std::string>());
        m_held_ac_bonus = yaml["weapon_data"]["ac"].as<int>();
    }
    if (yaml["attribute"]) {
        std::string attr = yaml["attribute"].as<std::string>();
        if (attr == "str") {
            m_modifier = MonsterAttributes::Str;
        } else if (attr == "int") {
            m_modifier = MonsterAttributes::Int;
        } else if (attr == "dex") {
            m_modifier = MonsterAttributes::Dex;
        } else if (attr == "con") {
            m_modifier = MonsterAttributes::Con;
        } else if (attr == "wis") {
            m_modifier = MonsterAttributes::Wis;
        } else if (attr == "chr") {
            m_modifier = MonsterAttributes::Chr;
        } else {
            throw file+" has bad attribute entry";
        }
    }
    if (yaml["skill"]) {
        m_skill = yaml["skill"].as<std::string>();
    }
    if (yaml["wearable"]) {
        std::string location = yaml["wearable"]["location"].as<std::string>();
        if (location == "body") {
            m_wearable = WearableSlots::Body;
        } else {
            throw file+" has bad wearable entry";
        }
        m_worn_ac_bonus = yaml["wearable"]["ac"].as<int>();
    }
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