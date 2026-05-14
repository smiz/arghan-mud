#include "actor.h"
#include "dice.h"
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <cctype>

static const std::string direction_name[6] = {
    "the north", "the south", "the east", "the west", "up", "down"
};
static const std::string reverse_direction_name[6] = {
    "the south", "the north", "the west", "the east", "below", "above"
};

Actor::Actor(
    Graph& graph,
    std::string file,
    bool load,
    std::string name,
    bool pc,
    const initial_stats_t* const stats):
Model(graph),
description(name+ " is here."),
name(name,true),
file(file),
strength(10),
dexterity(10),
constitution(10),
intelligence(10),
wisdom(10),
charisma(10),
hit_points(2),
damage(0),
armor_class(10),
ac_modifier(0),
fd(-1),
exit_node_id(START_GROUP),
pc(pc) {
    if (load) {
        YAML::Node yaml = YAML::LoadFile(file.c_str());
        this->name = Name(yaml["name"].as<std::string>(),yaml["proper_name"].as<bool>());
        description = yaml["description"].as<std::string>();
        detail = yaml["detail"].as<std::string>();
        const YAML::Node& keyword_list = yaml["keywords"];
        for (const auto& word : keyword_list) {
            key_words.push_back(word.as<std::string>());
        }
        if (yaml["items"]) {
            const YAML::Node& item_list = yaml["items"];
            for (const auto& item : item_list) {
                std::cout << item.as<std::string>() << std::endl;
                auto new_item = std::make_shared<Item>(item.as<std::string>());
                items.push_back(new_item);
            }
        }
        if (yaml["strength"]) {
            strength = yaml["strength"].as<int>();
        }
        if (yaml["dexterity"]) {
            dexterity = yaml["dexterity"].as<int>();
        }
        if (yaml["constitution"]) {
            constitution = yaml["constitution"].as<int>();
        }
        if (yaml["intelligence"]) {
            intelligence = yaml["intelligence"].as<int>();
        }
        if (yaml["wisdom"]) {
            wisdom = yaml["wisdom"].as<int>();
        }
        if (yaml["charisma"]) {
            charisma = yaml["charisma"].as<int>();
        }
        if (yaml["armor_class"]) {
            armor_class = yaml["armor_class"].as<int>();
        }
        if (yaml["hit_points"]) {
            hit_points = yaml["hit_points"].as<int>();
        }
    } else {
        init(stats);
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

initial_stats_t Actor::initial_stats() {
    initial_stats_t stats;
    Dice attr_die(3,6);
    stats.str = attr_die();
    stats.dex = attr_die();
    stats.con = attr_die();
    stats.intel = attr_die();
    stats.wis = attr_die();
    stats.chr = attr_die();
    stats.hp = std::max(1,4+attribute_modifier(stats.con));
    return stats;
}

void Actor::init(const initial_stats_t* const stats) {
    armor_class = 10;
    if (stats != nullptr) {
        strength = stats->str;
        dexterity = stats->dex;
        constitution = stats->con;
        intelligence = stats->intel;
        wisdom = stats->wis;
        charisma = stats->chr;
        hit_points = stats->hp;
    }
    description = name.capitalized_name()+" is here.";
    detail = name.capitalized_name()+" is pretty good lookin'!";
    key_words.push_back(name.get_name());
    key_words.push_back(name.lower_case());
}

void Actor::save() {
    if (!pc) {
        return;
    }
    YAML::Node config;
    config["name"] = name.get_name();
    config["proper_name"] = name.is_proper();
    config["description"] = description;
    config["detail"] = detail;
    config["keywords"] = key_words;
    config["strength"] = strength;
    config["dexterity"] = dexterity;
    config["constitution"] = constitution;
    config["intelligence"] = intelligence;
    config["wisdom"] = wisdom;
    config["charisma"] = charisma;
    config["armor_class"] = armor_class;
    config["hit_points"] = hit_points;
    std::vector<std::string> item_files;
    for (auto item: items) {
        item_files.push_back(item->filename());
    }
    config["items"] = item_files;
    std::ofstream fout(file.c_str());
    fout << config; 
    fout.close();
}

void Actor::report_stats() {
    std::string line;
    std::ostringstream sout(line);
    sout << "str " << strength << std::endl;;
    sout << "dex " << dexterity << std::endl;;
    sout << "con " << constitution << std::endl;;
    sout << "int " << intelligence << std::endl;;
    sout << "wis " << wisdom << std::endl;;
    sout << "chr " << charisma << std::endl;;
    sout << "hp  " << hit_points << " / " << damage << std::endl;;
    sout << "ac  " << armor_class+ac_modifier << std::endl;;
    message(sout.str());
}

void Actor::report_inventory() {
    std::string msg = "You have";
    if (items.empty()) {
        msg += " nothing.";
    } else {
        msg += ":";
        for (auto item: items) {
            msg += "\n" + item->name().capitalized_name();
        }
    }
    message(msg);
}

void Actor::message(std::string msg) {
    int bytes;
    static char newline = '\n';
    if (fd != -1) {
        if (msg.back() != '\n') {
            msg += newline;
        }
        bytes = write(fd,msg.c_str(),msg.size());
        if (bytes < 0) {
            fd = -1;
        }
    }
}

void Actor::change_prox_groups(int new_group) {

    Event leave, enter;
    leave.src_id = enter.src_id = id();
    leave.dst_id = enter.dst_id = id();

    leave.type = Event::LEAVE_PROX_GROUP;
    leave.event_data.prox_group = group->group_number();
    leave.pin = group->pin;

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
    enter.msg = name.capitalized_name()+" returns from the Real World!";
    sched_event(enter);
    if (pc) {
        sched_save();
    }
}

void Actor::leave_mud_event(const Event& event) {
    exit_node_id = group->group_number();
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
    leave.msg = name.capitalized_name()+" mutters something about the Real World and vanishes!";
    sched_event(leave);
}

void Actor::join_prox_group_event(const Event& event) {
    if (event.dst_id == id()) {
        group = prox_map[event.event_data.prox_group];
        group->add_member(this);
        receive_from(group->pin);
    } else {
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
    if (!group->find_direction(move_dir)) {
        message("You can't go in that direction.");
        return;
    }
    change_prox_groups(move_dir.id);
    Event see;
    see.type = Event::SEE;
    see.src_id = id();
    see.dst_id = ANY_ID_BUT_SRC;
    see.msg = name.capitalized_name()+" arrives from "+reverse_direction_name[move_dir.dir]+".";
    see.pin = prox_map[move_dir.id]->pin;
    sched_event(see);
    see.msg = name.capitalized_name()+" goes "+direction_name[move_dir.dir]+".";
    see.pin = group->pin;
    sched_event(see);
}

void Actor::see_event(const Event& event) {
    if (event.dst_id == id() || event.dst_id == ANY_ID || (event.dst_id == ANY_ID_BUT_SRC && event.src_id != id())) {
        message(event.msg);
    }
}

void Actor::transfer_item_event(const Event& event) {
    if (event.dst_id != id()) {
        return;
    }
    assert(event.event_data.transfer_src_to_dst);
    items.push_back(event.item);
    save();
}

void Actor::get_command_event(const Event& event) {
    if (event.dst_id != id()) {
        return;
    }
    Event get(event);
    get.type = Event::TRANSFER_ITEM;
    get.dst_id = group->first_member_id();
    get.pin = group->members().front()->pin;
    get.src_id = id();
    get.event_data.transfer_src_to_dst = false;
    sched_event(get);
}

void Actor::drop_command_event(const Event& event) {
    if (event.dst_id != id()) {
        return;
    }
    Event drop(event);
    Event see;
    see.type = Event::SEE1;
    see.src_id = id();
    see.dst_id = ANY_ID_BUT_SRC;
    see.pin = group->pin;
    drop.type = Event::TRANSFER_ITEM;
    drop.dst_id = group->first_member_id();
    drop.pin = group->members().front()->pin;
    drop.src_id = id();
    drop.event_data.transfer_src_to_dst = true;
    drop.item = find_item(*(event.key_words.get()));
    if (drop.item == nullptr) {
        message("You don't have that item.");
    } else {
        items.remove(drop.item);
        save();
        see.msg = name.capitalized_name()+ " drops " + drop.item->name().regular_name()+".";
        sched_event(drop);
        sched_event(see);
        message("You drop "+drop.item->name().regular_name()+".");
    }
}

void Actor::look_command_event(const Event& event) {
    if (event.dst_id != id()) {
        return;
    }
    Event look;
    look.type = Event::LOOK;
    look.src_id = id();
    look.dst_id = group->find_best_match(*(event.key_words));
    if (look.dst_id == group->first_member_id()) {
        look.dst_id = ANY_ID_BUT_SRC;
    }
    look.key_words = event.key_words;
    look.pin = group->pin;
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
    see.pin = group->pin;
    sched_event(see);
}

void Actor::sched_save() {
    Event save;
    save.type = Event::SAVE_MODEL;
    save.pin = pin;
    sched_event(save,5000);
}

void Actor::save_model_event(const Event& event) {
    save();
    /// Save every second
    sched_save();
}

int Actor::attribute_modifier(int attribute_score) {
    if (attribute_score < 1) {
        return -6;
    }
    switch(attribute_score) {
        case 1: return -5;
        case 2: return -4;
        case 3: return -4;
        case 4: return -3;
        case 5: return -3;
        case 6: return -2;
        case 7: return -2;
        case 8: return -1;
        case 9: return -1;
        case 10: return 0;
        case 11: return 0;
        case 12: return 1;
        case 13: return 1;
        case 14: return 2;
        case 15: return 2;
        case 16: return 3;
        case 17: return 3;
        case 18: return 4;
        case 19: return 4;
        case 20: return 5;
        case 21: return 5;
        case 22: return 6;
        case 23: return 6;
        case 24: return 7;
        case 25: return 7;
        case 26: return 8;
        case 27: return 8;
        case 28: return 9;
        case 29: return 9;
    }
    return 10;
}