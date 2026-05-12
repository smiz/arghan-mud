#include "actor.h"
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

static const std::string direction_name[6] = {
    "the north", "the south", "the east", "the west", "up", "down"
};
static const std::string reverse_direction_name[6] = {
    "the south", "the north", "the west", "the east", "below", "above"
};

Actor::Actor(Graph& graph, std::string file, bool load, std::string name, int exit_node):
Model(graph),
description(name+ " is here."),
name(name),
file(file),
fd(-1),
exit_node_id(exit_node) {
    if (load) {
        YAML::Node yaml = YAML::LoadFile(file.c_str());
        this->name = yaml["name"].as<std::string>();
        description = yaml["description"].as<std::string>();
        detail = yaml["detail"].as<std::string>();
        const YAML::Node& keyword_list = yaml["keywords"];
        for (const auto& word : keyword_list) {
            key_words.push_back(word.as<std::string>());
        }
    } else {
        init();
        save();
    }
}

int Actor::match_keywords(const KeyWordList& key_words) const {
    int score = 0;
    for (auto word: this->key_words) {
        if (std::find(key_words.begin(),key_words.end(),word) != key_words.end()) {
            score++;
        }
    }
    return score;
}

void Actor::init() {
    description = name+" is here.";
    detail = name+" is pretty good lookin'!";
    key_words.push_back(name);
}

void Actor::save() {
    YAML::Node config;
    config["name"] = name;
    config["description"] = description;
    config["detail"] = detail;
    config["keywords"] = key_words;
    std::ofstream fout(file.c_str());
    fout << config; 
    fout.close();
}

void Actor::message(std::string msg) {
    static char newline = '\n';
    if (fd != -1) {
        write(fd,msg.c_str(),msg.size());
        if (msg.back() != '\n') {
            write(fd,&newline,1);
        }
    }
}

void Actor::change_prox_groups(int new_group) {

    Event leave, enter;
    leave.src_id = enter.src_id = id();
    leave.dst_id = enter.dst_id = id();

    leave.type = Event::LEAVE_PROX_GROUP;
    leave.event_data.prox_group = prox_groups.front()->group_number();
    leave.pin = prox_groups.front()->pin;

    sched_event(leave);

    enter.type = Event::JOIN_PROX_GROUP;
    enter.event_data.prox_group = new_group; 
    enter.pin = prox_map[new_group]->pin;

    sched_event(enter);

    enter.pin = pin;

    sched_event(enter);
}

void Actor::enter_mud_event(const Event& event) {
    Event enter;
    enter.src_id = id();
    enter.dst_id = id();
    enter.type = Event::JOIN_PROX_GROUP;
    enter.event_data.prox_group = exit_node_id; 
    enter.pin = prox_map[exit_node_id]->pin;
    sched_event(enter);
    enter.pin = pin;
    sched_event(enter);
    enter.type = Event::SEE1;
    enter.pin = prox_map[exit_node_id]->pin;
    enter.dst_id = ANY_ID_BUT_SRC;
    enter.msg = name+" returns from the Real World!";
    sched_event(enter);
}

void Actor::leave_mud_event(const Event& event) {
    exit_node_id = prox_groups.front()->group_number();
    /// Leave all prox groups but stay in the game
    for (auto group: prox_groups) {
        Event leave;
        leave.type = Event::LEAVE_PROX_GROUP;
        leave.event_data.prox_group = group->group_number();
        leave.pin = group->pin;
        leave.src_id = id();
        leave.dst_id = id();
        sched_event(leave);
        leave.type = Event::SEE1;
        leave.pin = group->pin;
        leave.dst_id = ANY_ID_BUT_SRC;
        leave.msg = name+" mutters something about the Real World and vanishes!";
        sched_event(leave);
    }
}

void Actor::quit_mud_event(const Event& event) {
    leave_game();
}

void Actor::join_prox_group_event(const Event& event) {
    if (event.dst_id == id()) {
        ProximityGroup* group = prox_map[event.event_data.prox_group];
        prox_groups.push_back(group);
        group->add_member(this);
        receive_from(group->pin);
    } else {
        ProximityGroup* group = prox_groups.front();
        Event see;
        see.type = Event::SEE1;
        see.src_id = id();
        see.dst_id = ANY_ID_BUT_SRC;
        see.msg = description;
        see.pin = group->pin;
        sched_event(see);
    }
}

void Actor::leave_prox_group_event(const Event& event) {
    if (event.dst_id == id()) {
        ProximityGroup* group = prox_map[event.event_data.prox_group];
        prox_groups.remove(group);
        group->remove_member(this);
        do_not_receive_from(group->pin);
    }
}

void Actor::move_event(const Event& event) {
     if (event.dst_id != id()) {
        return;
    }
    direction_t move_dir;
    move_dir.dir = event.event_data.dir;
    ProximityGroup* group = prox_groups.front();
    if (!group->find_direction(move_dir)) {
        message("You can't go in that direction.");
        return;
    }
    change_prox_groups(move_dir.id);
    Event see;
    see.type = Event::SEE;
    see.src_id = id();
    see.dst_id = ANY_ID_BUT_SRC;
    see.msg = name+" arrives from "+reverse_direction_name[move_dir.dir]+".";
    see.pin = prox_map[move_dir.id]->pin;
    sched_event(see);
    see.msg = name+" goes "+direction_name[move_dir.dir]+".";
    see.pin = group->pin;
    sched_event(see);
}

void Actor::see_event(const Event& event) {
    if (event.dst_id == id() || event.dst_id == ANY_ID || (event.dst_id == ANY_ID_BUT_SRC && event.src_id != id())) {
        message(event.msg);
    }
}

void Actor::look_command_event(const Event& event) {
    if (event.dst_id != id()) {
        return;
    }
    Event look;
    look.type = Event::LOOK;
    look.src_id = id();
    look.dst_id = prox_groups.front()->find_best_match(*(event.key_words));
    if (look.dst_id == prox_groups.front()->first_member_id()) {
        look.dst_id = ANY_ID_BUT_SRC;
    }
    look.key_words = event.key_words;
    look.pin = prox_groups.front()->pin;
    sched_event(look);
}

void Actor::look_event(const Event& event) {
    if (event.dst_id != id() && event.dst_id != ANY_ID && !(event.dst_id == ANY_ID_BUT_SRC && event.src_id != id())) {
        return;
    }
    Event see;
    see.type = Event::SEE1;
    see.src_id = id();
    see.dst_id = event.src_id;
    if (event.dst_id == id()) {
        see.msg = detail; 
    } else {
        see.msg = description;
    }
    see.pin = prox_groups.front()->pin;
    sched_event(see);
}