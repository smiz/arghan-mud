#include "item.h"
#include <yaml-cpp/yaml.h>
#include <iostream>

const std::string Item::items_directory = "items/";

Item::Item(std::string file):
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