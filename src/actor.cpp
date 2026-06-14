#include "actor.h"
#include "dice.h"
#include "corpse.h"
#include "coins.h"
#include "utils.h"
#include <filesystem>
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <cctype>

static const std::string direction_name[6] = {
    "north", "south", "east", "west", "up", "down"
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
    const initial_stats_t* const stats,
    int regen_room_number):
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
wanders(0),
aggressive(false),
sneaking(0),
unarmed_dmg(0,1),
level(0),
xp_to_go(INT_MAX),
free_skill_slots(0),
fd(-1),
exit_node_id(regen_room_number),
regen_node_number(regen_room_number),
pc(pc),
short_descriptions(false) {
    if (load) {
        YAML::Node yaml = YAML::LoadFile(file.c_str());
        this->name = Name(yaml["name"].as<std::string>(),yaml["proper_name"].as<bool>());
        description = yaml["description"].as<std::string>();
        detail = yaml["detail"].as<std::string>();
        const YAML::Node& keyword_list = yaml["keywords"];
        for (const auto& word : keyword_list) {
            key_words.push_back(word.as<std::string>());
        }
        if (yaml["attack_dice"]) {
            unarmed_dmg = Dice(yaml["attack_dice"].as<std::string>());
        }
        if (yaml["aggressive"]) {
            aggressive = yaml["aggressive"].as<bool>();
        }
        if (yaml["wear_body"]) {
            body = std::make_shared<Item>(yaml["wear_body"].as<std::string>());
        }
        if (yaml["wear_neck"]) {
            neck = std::make_shared<Item>(yaml["wear_neck"].as<std::string>());
        }
        if (yaml["wield"]) {
            primary_hand = std::make_shared<Item>(yaml["wield"].as<std::string>());
        }
        if (yaml["hold"]) {
            secondary_hand = std::make_shared<Item>(yaml["hold"].as<std::string>());
        }
        if (yaml["assist"]) {
            const YAML::Node& assist_list = yaml["assist"];
            for (const auto& pal : assist_list) {
                assist.insert(pal.as<std::string>());
            }
        }
        if (yaml["items"]) {
            const YAML::Node& item_list = yaml["items"];
            for (const auto& item : item_list) {
                std::cout << item.as<std::string>() << std::endl;
                auto new_item = std::make_shared<Item>(item.as<std::string>());
                items.push_back(new_item);
            }
        }
        if (yaml["skills"]) {
            auto skill_map = yaml["skills"].as<std::map<std::string,int>>();
            for (auto const& [key, val] : skill_map) {
                skills[to_skill(key)] = val;
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
        if (yaml["wanders"]) {
            wanders = yaml["wanders"].as<int>();
            if (wanders > 0) {
                Event event(Event::WANDER,id());
                event.dst_id = id();
                event.pin = pin;
                sched_event(event,wanders);
            }
        }
        if (yaml["xp_to_go"]) {
            xp_to_go = yaml["xp_to_go"].as<int>();
        }
        if (yaml["level"]) {
            level = yaml["level"].as<int>();
        }
        if (yaml["free_slots"]) {
            free_skill_slots = yaml["free_slots"].as<int>();
        }
        if (yaml["password"]) {
            password = yaml["password"].as<std::string>();
        }
        if (yaml["rumors"]) {
            const YAML::Node& rumor_list = yaml["rumors"];
            for (const auto& rumor : rumor_list) {
                rumors.push_back(rumor.as<std::string>());
            }
        }
    } else {
        init(stats);
        save();
    }
    Event periodic(Event::ROLL_PERIODIC_ATTRIBUTES,id());
    periodic.dst_id = id();
    periodic.pin = pin;
    sched_event(periodic,1+rand()%60000);
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

void Actor::gain_xp(int xp) {
    Dice hp_die(1,6);
    while (xp > xp_to_go) {
        level++;
        free_skill_slots++;
        xp -= xp_to_go;
        xp_to_go = level*20;
        hit_points += std::max(1,hp_die()+attribute_modifier(constitution));
        message("You gained a level!");
        for (auto& skill: skills) {
            skill.second++;
        }
    }
    xp_to_go -= xp;
}

initial_stats_t Actor::initial_stats() {
    initial_stats_t stats;
    Dice hp_die(1,6);
    Dice attr_die(3,6);
    stats.str = attr_die();
    stats.dex = attr_die();
    stats.con = attr_die();
    stats.intel = attr_die();
    stats.wis = attr_die();
    stats.chr = attr_die();
    stats.hp = std::max(1,hp_die()+attribute_modifier(stats.con));
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
    xp_to_go = 10;
    description = name.capitalized_name()+" is here.";
    detail = name.capitalized_name()+" is pretty good lookin'!";
    key_words.push_back(name.get_name());
    key_words.push_back(name.lower_case());
}

void Actor::build_inventory(std::shared_ptr<Item>& item, std::vector<std::string>& item_files) {
    /// Don't include transient items (specifically, corpses and coins)
    if (!item->filename().empty()) {
        std::ostringstream sout;
        sout << "characters/" << name.get_name() << "-items/item-" << item_files.size();
        item->set_filename("items/"+sout.str());
        item->save();
        item->set_filename(sout.str());
        item_files.push_back(item->filename());
    }
    if (!item->container()) {
        return;
    }
    for (auto item: item->contents()) {
        build_inventory(item,item_files);
    }
}

void Actor::save() {
    if (!pc) {
        return;
    }
    YAML::Node config;
    if (!password.empty()) {
        config["password"] = password;
    }
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
    config["level"] = level;
    config["xp_to_go"] = xp_to_go;
    config["free_slots"] = free_skill_slots;
    std::filesystem::path item_path = "items/characters/"+name.get_name()+"-items";
    std::filesystem::remove_all(item_path);
    std::filesystem::create_directories(item_path);
    std::vector<std::string> item_files;
    for (auto item: items) {
        build_inventory(item,item_files);
    }
    if (primary_hand != nullptr) {
        build_inventory(primary_hand,item_files);
    }
    if (secondary_hand != nullptr) {
        build_inventory(secondary_hand,item_files);
    }
    if (body != nullptr) {
        build_inventory(body,item_files);
    }
    if (neck != nullptr) {
        build_inventory(neck,item_files);
    }
    config["items"] = item_files;
    std::map<std::string,int> skill_table;
    for (auto skill: skills) {
        skill_table[from_skill(skill.first)] = skill.second;
    }
    config["skills"] = skill_table;
    std::ofstream fout(file.c_str());
    fout << config; 
    fout.close();
}

void Actor::report_skills() {
    std::string line;
    std::ostringstream sout(line);
    if (skills.empty()) {
        sout << "You have no skills.\n";
    } else {
        sout << "Your skills:" << std::endl;
        for (auto skill: skills) {
            sout << from_skill(skill.first) << " " << skill.second << "\n";
        }
    }
    if (free_skill_slots > 0) {
        sout << "You have " << free_skill_slots << " free skill slots." << std::endl;
    }
    message(sout.str());
}

void Actor::report_stats() {
    std::string line;
    std::ostringstream sout(line);
    sout << "level " << level << " , " << xp_to_go << " xp to next level" << std::endl;
    sout << "str   " << strength << std::endl;;
    sout << "dex   " << dexterity << std::endl;;
    sout << "con   " << constitution << std::endl;;
    sout << "int   " << intelligence << std::endl;;
    sout << "wis   " << wisdom << std::endl;;
    sout << "chr   " << charisma << std::endl;;
    sout << "hp    " << hit_points << " / " << damage << std::endl;;
    sout << "ac    " << total_ac() << std::endl;;
    message(sout.str());
}

void Actor::report_inventory() {
    std::string msg = "In your pack you have";
    if (items.empty()) {
        msg += " nothing.";
    } else {
        msg += ":";
        for (auto item: items) {
            msg += "\n" + item->name().capitalized_name();
        }
    }
    message(msg);
    if (primary_hand != nullptr) {
        message("You wield "+primary_hand->name().regular_name()+".");
    }
    if (secondary_hand != nullptr) {
        message("You hold "+secondary_hand->name().regular_name()+".");
    }
    if (body != nullptr) {
        message("You are clothed in "+body->name().regular_name()+".");
    }
    if (neck != nullptr) {
        message("You wear "+neck->name().regular_name()+" around your neck.");
    }
}

void Actor::message(std::string msg) {
    static const char newline = '\n';
    if (fd != -1) {
        // Make sure we end with one newline
        while (msg.back() == newline) {
            msg.pop_back();
        }
        msg += newline;
        if (write(fd,msg.c_str(),msg.size()) < 0) {
            fd = -1;
        }
    }
}

void Actor::emit_stealthy(std::string msg, int dst_id, int exclude) {
    Event event(Event::SEE1,id());
    event.event_data.subject_id = id();
    event.msg = msg;
    event.dst_id = dst_id;
    event.pin = group->pin;
    event.stealthy = sneaking;
    if (exclude >= 0) {
        event.exclude = std::make_shared<std::set<int>>();
        event.exclude->insert(exclude);
    }
    sched_event(event);
}

void Actor::emit(std::string msg, int dst_id, int exclude) {
    Event event(Event::SEE1,id());
    event.event_data.subject_id = id();
    event.msg = msg;
    event.dst_id = dst_id;
    event.pin = group->pin;
    event.stealthy = 0;
    if (exclude >= 0) {
        event.exclude = std::make_shared<std::set<int>>();
        event.exclude->insert(exclude);
    }
    sched_event(event);
}

void Actor::sneak_command_event(const Event& event) {
    bool combat = in_combat();
    if (combat) {
        message("You are fighting for your life!");
        return;
    }
    if (event.stealthy != 0) {
        if (sneaking == 0) {
            message("You disappear into the shadows.");
            emit(name.capitalized_name()+" disappears into the shadows.");
        } else {
            message("You attempt to stay hidden.");
            emit_stealthy(name.capitalized_name()+" attempts to stay hidden.");
        }
        sneaking = use_skill(Stealth);
    } else {
        sneaking = 0;
        message("You emerge from hiding.");
        emit(name.capitalized_name()+" emerges from hiding.");
    }
}

void Actor::swindle_result_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    gain_xp(event.item->claim_xp());
    items.push_back(event.item);
    message("You con the mark and get "+event.item->name().regular_name()+"!");
}

void Actor::start_swindle_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    if (sneaking > event.perceptive) {
        Model::start_swindle_event(event);
        return;
    }
    Name swindler_name = group->find_member(event.src_id)->get_name();
    emit(swindler_name.capitalized_name()+ " whispers something to " + name.regular_name()+".",
        ANY_ID_BUT_SRC,event.src_id);
    Event result(Event::SEE1,id());
    result.event_data.subject_id = id();
    result.dst_id = event.src_id;
    result.pin = group->pin;
    result.msg = "You attempt to swindle "+name.regular_name()+" ...";
    sched_event(result);
    Event swindle(event);
    swindle.type = Event::SWINDLE;
    swindle.dst_id = id();
    swindle.pin = group->pin;
    sched_event(swindle,1000);
}

std::shared_ptr<Item> Actor::find_any_item(const KeyWordList& key_words) {
    int match = 0;
    auto item = Model::find_item(key_words);
    if (item != nullptr) {
        match = item->match_keywords(key_words);
    }
    if (body != nullptr) {
        int tmp = body->match_keywords(key_words);
        if (tmp > 0) {
            match = tmp;
            item = body;
        }
    }
    if (neck != nullptr) {
        int tmp = neck->match_keywords(key_words);
        if (tmp > match) {
            match = tmp;
            item = neck;
        }
    }
    if (primary_hand != nullptr) {
        int tmp = primary_hand->match_keywords(key_words);
        if (tmp > match) {
            match = tmp;
            item = primary_hand;
        }
    }
    if (secondary_hand != nullptr) {
        int tmp = secondary_hand->match_keywords(key_words);
        if (tmp > match) {
            match = tmp;
            item = secondary_hand;
        }
    }
    auto alt_item = find_item(key_words);
    if (alt_item != nullptr) {
        int tmp = alt_item->match_keywords(key_words);
        if (tmp > match) {
            item = alt_item;
        }
    }
    return item;
}

void Actor::swindle_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    bool combat = in_combat();
    if (combat) {
        Event result(Event::SEE1,id());
        result.event_data.subject_id = id();
        result.dst_id = event.src_id;
        result.msg = name.capitalized_name()+" is too busy to talk to you!";
        result.pin = group->pin;
        sched_event(result);
        return;
    }
    std::shared_ptr<Item> item = find_any_item(*(event.key_words.get()));

    if (item == nullptr) {
        Event result(Event::SEE1,id());
        result.event_data.subject_id = id();
        result.dst_id = event.src_id;
        result.pin = group->find_member(event.src_id)->pin;
        result.msg = "You don't see that.";
        sched_event(result);
        return;
    }
    auto swindler = group->find_member(event.src_id);
    if (swindler == nullptr) {
        return;
    }
    if (check_skill(Perception,event.event_data.swindling+item->get_cost()/5,true)) {
        Event result(Event::SWINDLE_RESULT,id());
        result.dst_id = event.src_id;
        result.pin = group->pin;
        result.item = item;
        message(swindler->get_name().capitalized_name()+" talks you out of "+item->name().regular_name()+".");
        emit(name.capitalized_name()+" gives "+item->name().regular_name()+" to "+swindler->get_name().regular_name()+".",
            ANY_ID_BUT_SRC,event.src_id);
        if (item == neck) {
            neck = nullptr;
        } else if (item == body) {
            body = nullptr;
        } else if (item == primary_hand) {
            primary_hand = nullptr;
        } else if (item == secondary_hand) {
            secondary_hand = nullptr;
        } else {
            items.remove(item);
        }
        sched_event(result);
    } else {
        message(swindler->get_name().capitalized_name()+" attempts to swindle you!");
        if (!pc) {
            schedule_attack(event.src_id,true);
        }
    }
}

void Actor::swindle_command_event(const Event& event) {
    bool combat = in_combat();
    if (combat) {
        message("You are fighting for your life!");
        return;
    }
    int target = group->find_best_match(*(event.key_words2.get()));
    Event swindle(event);
    swindle.type = Event::START_SWINDLE;
    swindle.pin = group->pin;
    swindle.dst_id = target;
    swindle.perceptive = use_skill(Perception,true);
    swindle.event_data.swindling = use_skill(Swindle);
    if (sneaking > 0) {
        sneaking = 0;
        message("You emerge from hiding.");
        emit(name.capitalized_name()+" emerges from hiding.");
    }
    sched_event(swindle);
}

void Actor::hear_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    message(event.msg);
}

void Actor::roll_periodic_attributes_event(const Event& event) {
    Event again(event);
    sched_event(again,60000-1000+rand()%2000);
    if (!rumors.empty()) {
        Event rumor(Event::HEAR,id());
        rumor.dst_id = ANY_ID_BUT_SRC;
        rumor.pin = group->pin;
        rumor.msg = name.capitalized_name()+" says \"" + rumors[rand()%rumors.size()] + "\"";
        sched_event(rumor);
    }
    if (damage > 0) {
        damage--;
    }
}

void Actor::change_prox_groups(int new_group) {

    Event leave, enter;
    leave.src_id = enter.src_id = id();
    leave.dst_id = enter.dst_id = id();

    leave.type = Event::LEAVE_PROX_GROUP;
    leave.event_data.prox_group = group->group_number();
    leave.pin = group->pin;
    leave.stealthy = sneaking;

    sched_event(leave);

    enter.type = Event::JOIN_PROX_GROUP;
    enter.event_data.prox_group = new_group; 
    enter.stealthy = sneaking;
    enter.pin = prox_map[new_group]->pin;

    sched_event(enter);

    enter.pin = pin;

    sched_event(enter);
}

void Actor::practice_event(const Event& event) {
    for (auto skill: *(event.key_words)) {
        Skill code = NoSkill;
        try {
            code = to_skill(skill);
        } catch(...) {

        }
        if (code == NoSkill) {
            std::string msg = skill+" is not an art known to humankind.";
            message(msg);
        } else if (free_skill_slots > 0) {
            if (skills.find(code) == skills.end()) {
                skills[code] = 0;
            }
            skills[code] = skills[code] + 1;
            free_skill_slots--;
            std::string msg = "You are better at " + skill+"!";
            message(msg);
        } else {
            message("You need more experience!");
            return;
        }
    }
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
    enter.event_data.subject_id = id();
    enter.pin = prox_map[exit_node_id]->pin;
    enter.dst_id = ANY_ID_BUT_SRC;
    enter.msg = name.capitalized_name()+" returns from the Real World!";
    sched_event(enter);
    // Cancel and restart periodic events
    cancel_event(Event::ROLL_PERIODIC_ATTRIBUTES);
    cancel_event(Event::SAVE_MODEL);
    Event periodic(Event::ROLL_PERIODIC_ATTRIBUTES,id());
    periodic.dst_id = id();
    periodic.pin = pin;
    sched_event(periodic,1+rand()%60000);
    if (pc) {
        sched_save();
    }
}

std::pair<std::string,std::string> Actor::damage_adjective() {
    std::pair<std::string,std::string> adj;
    double fraction = double(damage) / double(hit_points);
    if (damage >= hit_points) {
        adj.first = "kills";
        adj.second = "kill";
    } else if (fraction > 0.9) {
        adj.first = "critically injures";
        adj.second = "critically injure";
    } else if (fraction > 0.75) {
        adj.first = "seriously injures";
        adj.second = "seriously injure";
    } else if (fraction > 0.5) {
        adj.first = "badly wounds";
        adj.second = "badly wound";
    } else if (fraction > 0.25) {
        adj.first = "lightly wounds";
        adj.second = "lightly wound";
    } else if (fraction > 0.1) {
        adj.first = "barely wounds";
        adj.second = "barely wound";
    } else {
        adj.first = "scratches";
        adj.second = "scratch";
    }
    return adj;
}

void Actor::melee_attack_event(const Event& event) {
    auto target = group->find_member(event.dst_id);
    if (target != nullptr && assist.contains(target->get_name().get_name()) && !in_combat()) {
        schedule_attack(event.src_id,false);
    }
    if (filter(event)) {
        return;
    }
    Event result(event);
    result.type = Event::MELEE_RESULT;
    result.src_id = id();
    result.dst_id = event.src_id;
    result.event_data.melee.killed = false;
    auto attacker = group->find_member(event.src_id);
    if (attacker == nullptr) {
        return;
    }
    result.pin = group->pin;
    if (event.event_data.melee.atk_roll >= total_ac()) {
        std::string wpn_name = "";
        std::string adj1, adj2;
        if (event.item != nullptr) {
            wpn_name = event.item->name().regular_name();
        }
        damage += std::min(event.event_data.melee.dmg_roll,hit_points-damage);
        result.event_data.melee.dmg_roll = damage;
        auto adj = damage_adjective();
        if (damage >= hit_points) {
            result.event_data.melee.killed = true;
        }
        std::string msg1(attacker->get_name().capitalized_name()+" "+adj.first+" ");
        std::string msg2;
        if (wpn_name.empty()) {
            msg2 = "!";
        } else {
            msg2 = " with "+wpn_name+"!";
        }
        emit(msg1+name.regular_name()+msg2,ANY_ID_BUT_SRC,attacker->id());
        emit("You "+adj.second+" "+name.regular_name()+msg2,attacker->id());
        message(msg1+"you"+msg2);
        if (!pc) {
            hates.insert(attacker->id());
        }
    } else {
        result.event_data.melee.dmg_roll = 0;
        std::string msg = attacker->get_name().capitalized_name() + " misses you!";
        message(msg);
        msg = attacker->get_name().capitalized_name() + " misses " + name.regular_name()+"!";
        emit(msg,ANY_ID_BUT_SRC,attacker->id());
        msg = "You miss " + name.regular_name()+"!";
        emit(msg,attacker->id());
    } 
    // Stop sneaking in combat
    sneaking = 0;
    /// Get our combat status before scheduling a RESULT
    bool engaged = in_combat();
    sched_event(result);
    if (damage >= hit_points) {
        schedule_destroyed();
        return;
    }
    // If we are not in combat already (we were surprised for example)
    if (!engaged) {
        schedule_attack(event.src_id,false);
    }
}

void Actor::schedule_destroyed() {
    Event event(Event::DESTROYED,id());
    event.pin = pin;
    event.dst_id = id();
    sched_event(event); 
    event.pin = group->pin;
    event.dst_id = group->group_number();
    event.item = std::make_shared<Corpse>(name);
    while (!items.empty()) {
        event.item->add_item(items.front());
        items.pop_front();
    }
    if (primary_hand != nullptr) {
        event.item->add_item(primary_hand);
        primary_hand = nullptr;
    }
    if (secondary_hand != nullptr) {
        event.item->add_item(secondary_hand);
        secondary_hand = nullptr;
    }
    if (body != nullptr) {
        event.item->add_item(body);
        body = nullptr;
    }
    if (neck != nullptr) {
        event.item->add_item(neck);
        neck = nullptr;
    }
    sched_event(event); 
}

void Actor::reset_zone_event(const Event& event) {
    assert(!pc || !group->zone_is_empty());
    if (!group->zone_is_empty() || pc) {
        return;
    }
    group->remove_member(this);
    do_not_receive_from(group->pin);
    Model::leave_game();
}

void Actor::destroyed_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    group->remove_member(this);
    do_not_receive_from(group->pin);
    if (pc) {
        sneaking = 0;
        group = prox_map[regen_node_number];
        receive_from(group->pin);
        group->add_member(this);
        emit(name.capitalized_name()+" suddenly appears!");
        message("Ashes to ashes...dust to dust...and then remade.");
        message("Try looking around.");
        exit_node_id = regen_node_number;
        damage = 0;
    } else {
        Model::leave_game();
    }
}

void Actor::melee_result_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    // Get an XP for each point of damage that we do
    gain_xp(event.event_data.melee.dmg_roll);
    if (event.event_data.melee.killed) {
        if (!pc) {
            hates.erase(event.src_id);
        }
        return;
    }
    schedule_attack(event.src_id,false);
}

void Actor::kill_command_event(const Event& event) {
    std::string msg1, msg2;
    int target_id = group->find_best_match(*(event.key_words.get()));
    if (target_id == group->first_member_id() || 
            !check_skill(Perception,group->find_member(target_id)->hidden(),true)) {
            message("You don't see anything like that.");
            return;
    } else {
        if (in_combat()) {
            message("You are fighting for your life!");
            return;
        }
        schedule_attack(target_id,true);
        if (sneaking > 0) {
            message("You creep up on your prey...");
        } else {
            message("You rush to the attack!");
        }
        msg1 = name.capitalized_name()+" attacks ";
        if (primary_hand != nullptr) {
            msg2 = " with "+primary_hand->name().regular_name();
        }
        msg2 += "!";
        emit_stealthy(msg1+group->find_member(target_id)->get_name().regular_name()+msg2,ANY_ID_BUT_SRC,target_id);
        emit_stealthy(msg1+"you"+msg2,target_id);
    }
}

void Actor::schedule_attack(int target_id, bool warn) {
    // Cancel any swindling attempts
    cancel_event(Event::SWINDLE);
    cancel_event(Event::START_SWINDLE);
    // Cancel any pending attack before starting another
    cancel_event(Event::MELEE_ATTACK);
    // Start the attack
    Event attack(Event::MELEE_ATTACK,id());
    attack.dst_id = target_id;
    attack.pin = group->pin;
    if (primary_hand != nullptr) {
        attack.event_data.melee.dmg_roll =
            std::max(1,primary_hand->weapon_info().dmg_die()+attribute_modifier(strength));
            attack.event_data.melee.atk_roll = use_item(primary_hand);
    } else {
        attack.event_data.melee.dmg_roll = std::max(1,unarmed_dmg()+attribute_modifier(strength));
        attack.event_data.melee.atk_roll = std::max(1,Dice(1,20)()+attribute_modifier(strength));
    }
    attack.item = primary_hand;
    sched_event(attack,melee_attack_delay(primary_hand));
    if (warn) {
        Event event(Event::PENDING_ATTACK,id());
        event.dst_id = target_id;
        event.pin = group->pin;
        event.stealthy = sneaking;
        sched_event(event);
        Name victim_name = group->find_member(target_id)->get_name();
        emit_stealthy(name.capitalized_name()+" moves to attack " +victim_name.regular_name()+"!",ANY_ID_BUT_SRC,target_id);
    }
}

void Actor::pending_attack_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    if (!in_combat() && (event.stealthy == 0 || check_skill(Perception,event.stealthy,true))) {
        schedule_attack(event.src_id,false);
        auto attack_name = group->find_member(event.src_id)->get_name();
        message(attack_name.capitalized_name()+" is attacking you!");
    }

}

void Actor::leave_mud_event(const Event& event) {
    exit_node_id = group->group_number();
    Event leave(Event::LEAVE_PROX_GROUP,id());
    leave.event_data.prox_group = group->group_number();
    leave.pin = group->pin;
    leave.dst_id = id();
    sched_event(leave);
    cancel_event(Event::ROLL_PERIODIC_ATTRIBUTES);
    cancel_event(Event::SAVE_MODEL);
    emit_stealthy(name.capitalized_name()+" mutters something about the Real World and vanishes!");
}

bool Actor::act_if_hostile(const Event& event) {
    auto target = group->find_member(event.event_data.subject_id);
    if (target == nullptr || assist.contains(target->get_name().get_name())) {
        return false;
    }
    if (!in_combat() && (aggressive || hates.contains(event.event_data.subject_id))
        && (event.stealthy == 0 || check_skill(Perception,event.stealthy,true))) {
            schedule_attack(event.event_data.subject_id,true);
            return true;
    }
    return false;
}

void Actor::join_prox_group_event(const Event& event) {
    if (event.dst_id == id()) {
        group = prox_map[event.event_data.prox_group];
        group->add_member(this);
        receive_from(group->pin);
        if (damage >= hit_points) {
            schedule_destroyed();
        }
    } else {
        if (act_if_hostile(event)) {
            emit_stealthy(description+
                "\n"+name.capitalized_name()+
                " looks murderously at you.",event.src_id);
        } else {
            emit_stealthy(description,event.src_id);
        }
    }
}

void Actor::leave_prox_group_event(const Event& event) {
    if (event.dst_id == id()) {
        ProximityGroup* group = prox_map[event.event_data.prox_group];
        group->remove_member(this);
        do_not_receive_from(group->pin);
    }
}

void Actor::wander_event(const Event& event) {
    if (wanders <= 0) {
        return;
    }
    direction_t dir;
    do {
        dir.dir = group->random_exit();
        group->find_direction(dir);
    } while (dir.skill != NoSkill);
    if (prox_map[dir.id]->zone_number() == group->zone_number()) {
        Event move_event(Event::MOVE,id());
        move_event.dst_id = id();
        move_event.pin = group->pin;
        move_event.event_data.dir = dir.dir;
        sched_event(move_event);
    }
    Event wander_event(event);
    // Add a little noise to prevent weird synchrony of motion
    sched_event(wander_event,std::max(1,wanders-10+rand()%20));
}

void Actor::move_event(const Event& event) {
     if (filter(event)) {
        return;
    }
    direction_t move_dir;
    bool combat = in_combat();
    move_dir.dir = event.event_data.dir;
    if (combat && move_dir.dir != Direction::Flee) {
        message("You can't escape that way!");
        return;
    } else if (!combat && move_dir.dir == Direction::Flee) {
        message("You aren't in combat.");
        return;
    } else if (combat && move_dir.dir == Direction::Flee) {
        int tries = 0;
        do {
            move_dir.dir = group->random_exit();
            group->find_direction(move_dir);
            tries++;
        } while (move_dir.skill != NoSkill && tries < 10);
        if (move_dir.skill != NoSkill || Dice(1,20)()+attribute_modifier(dexterity) < 10) {
            message("You couldn't escape!");
            return;
        }
    }
    if (!group->find_direction(move_dir)) {
        message("You can't go in that direction.");
        return;
    }
    if (move_dir.skill != NoSkill) {
        emit_stealthy(name.capitalized_name()+" attempts to go "+direction_name[move_dir.dir]);
        if (!check_skill(move_dir.skill,move_dir.difficulty)) {
            emit_stealthy(name.capitalized_name()+" "+move_dir.fail_msg1);
            message("You "+move_dir.fail_msg2);
            if (move_dir.dmg != nullptr) {
                damage += (*(move_dir.dmg.get()))();
                message("That "+damage_adjective().first+" you!");
            }
            if (!move_dir.go_on_fail) {
                if (damage >= hit_points) {
                    schedule_destroyed();
                }
                return;
            }
        } else {
            emit_stealthy(name.capitalized_name()+" "+move_dir.success_msg1);
            message("You "+move_dir.success_msg2);
        }
    }
    change_prox_groups(move_dir.id);
    Event see(Event::SEE1,id());
    see.event_data.subject_id = id();
    see.dst_id = ANY_ID_BUT_SRC;
    see.msg = name.capitalized_name()+" arrives from "+reverse_direction_name[move_dir.dir]+".";
    see.pin = prox_map[move_dir.id]->pin;
    see.stealthy = sneaking;
    sched_event(see);
    if (event.event_data.dir != Direction::Flee) {
        see.msg = name.capitalized_name()+" goes "+direction_name[move_dir.dir]+".";
    } else {
        see.msg = name.capitalized_name()+" flees "+direction_name[move_dir.dir]+".";
    }
    see.pin = group->pin;
    sched_event(see);
}

void Actor::see_event(const Event& event) {
    if (!filter(event)) {
        if (event.stealthy == 0 || check_skill(Perception,event.stealthy,true)) {
            if (short_descriptions && (event.flags & SEE_SHORT)) {
                message(event.msg);
            } else if (!short_descriptions && (event.flags & SEE_LONG)) {
                message(event.msg);
            }
            act_if_hostile(event);
        }
    }
}

void Actor::transfer_item_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    assert(event.event_data.transfer.src_to_dst);
    gain_xp(event.item->claim_xp());
    // If we didn't have the money for it we won't receive it
    account_for_cost(event.item,false);
    items.push_back(event.item);
    save();
    consolidate_coins();
}

void Actor::get_command_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    if (in_combat()) {
        message("You are fighting for your life!");
        return;
    }
    Event get(event);
    get.type = Event::TRANSFER_ITEM;
    get.dst_id = group->first_member_id();
    get.pin = group->members().front()->pin;
    get.event_data.transfer.coins_in_purse = consolidate_coins();
    get.src_id = id();
    get.event_data.transfer.src_to_dst = false;
    sched_event(get);
}

void Actor::drop_command_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    if (in_combat()) {
        message("You are fighting for your life!");
        return;
    }
    Event drop(event);
    drop.type = Event::TRANSFER_ITEM;
    drop.dst_id = group->first_member_id();
    drop.pin = group->members().front()->pin;
    drop.src_id = id();
    drop.item = find_any_item(*(event.key_words.get()));
    drop.event_data.transfer.src_to_dst = true;
    // We have something
    if (drop.item != nullptr) {
        // On our person
        if (std::find(items.begin(),items.end(),drop.item) != items.end()) {
            items.remove(drop.item);
        } else if (drop.item == primary_hand) {
            primary_hand = nullptr;
        } else if (drop.item == secondary_hand) {
            secondary_hand = nullptr;
        } else if (drop.item == neck) {
            neck = nullptr;
        } else if (drop.item == body) {
            body = nullptr;
        }
        save();
        sched_event(drop);
        // Drop it in a shop means you sold it
        account_for_cost(drop.item,true);
    } else {
        message("You don't have that.");
    }
}

void Actor::look_command_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    Event look(event);
    look.type = Event::LOOK;
    look.src_id = id();
    /// Look at everything in the room
    if (event.key_words->empty() && !event.event_data.transfer.first_keyword_is_container) {
        look.dst_id = ANY_ID_BUT_SRC;
    } else if (event.event_data.transfer.first_keyword_is_container) {
        look.dst_id = group->first_member_id();
    } else {
        /// Are we carrying it?
        auto item = find_any_item(*(event.key_words.get()));
        if (item != nullptr) {
            message(item->detail());
            return;
        }
        /// Look at or in something specific in the room
        look.dst_id = group->find_best_match(*(event.key_words));
        if (!check_skill(Perception,group->find_member(look.dst_id)->hidden(),true)) {
            message("You don't see anything like that.");
            return;
        }
    }
    look.pin = group->pin;
    sched_event(look);
}

void Actor::look_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    if (event.event_data.transfer.first_keyword_is_container) {
        return;
    }
    Event see(Event::SEE1,id());
    see.event_data.subject_id = id();
    see.dst_id = event.src_id;
    if (event.dst_id == id()) {
        see.msg = detail+'\n'; 
        if (primary_hand != nullptr) {
            see.msg += name.capitalized_name()+" wields "+primary_hand->name().regular_name()+".\n";
        }
        if (secondary_hand != nullptr) {
            see.msg += name.capitalized_name()+" holds "+secondary_hand->name().regular_name()+".\n";
        }
        if (body != nullptr) {
            see.msg += name.capitalized_name()+" wears "+body->name().regular_name()+".\n";
        }
        if (neck != nullptr) {
            see.msg += name.capitalized_name()+" wears "+neck->name().regular_name()+".\n";
        }
        if (!items.empty()) {
            see.msg += name.capitalized_name()+" is carrying:\n";
            for (auto item: items) {
                see.msg += item->name().capitalized_name()+"\n";
            }
        }
    } else {
        see.msg = description;
    }
    if (see.msg.back() != '\n') {
            see.msg += '\n';
    }
    see.stealthy = sneaking;
    see.pin = group->pin;
    sched_event(see);
}

void Actor::stow_command_event(const Event& event) {
    std::shared_ptr<Item> best_slot;
    int best_score = 0, score;
    if (in_combat()) {
        message("You are fighting for your life!");
        return;
    }
    if (primary_hand != nullptr) {
        score = primary_hand->match_keywords(*(event.key_words.get())); 
        if (score > best_score) {
            best_score = score;
            best_slot = primary_hand;
        }
    }
    if (secondary_hand != nullptr) {
        score = secondary_hand->match_keywords(*(event.key_words.get())); 
        if (score > best_score) {
            best_score = score;
            best_slot = secondary_hand;
        }
    }
    if (body != nullptr) {
        score = body->match_keywords(*(event.key_words.get())); 
        if (score > best_score) {
            best_score = score;
            best_slot = body;
        }
    }
    if (neck != nullptr) {
        score = neck->match_keywords(*(event.key_words.get())); 
        if (score > best_score) {
            best_score = score;
            best_slot = neck;
        }
    }
    if (best_slot == nullptr) {
        message("You aren't holding or wearing that.");
    } else {
        items.push_back(best_slot);
        if (best_slot == primary_hand) {
            message("You stow "+primary_hand->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" stows "+primary_hand->name().regular_name()+".");
            primary_hand = nullptr; 
        } else if (best_slot == secondary_hand) {
            message("You stow "+secondary_hand->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" stows "+secondary_hand->name().regular_name()+".");
            secondary_hand = nullptr;
        } else if (best_slot == neck) {
            message("You stow "+neck->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" stows "+neck->name().regular_name()+".");
            neck = nullptr;
        } else {
            message("You remove "+body->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" removes "+body->name().regular_name()+".");
            body = nullptr;
        }
    }
}

void Actor::read_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    int literacy = use_skill(Literacy,true);
    auto item = find_any_item(*(event.key_words.get()));
    if (item == nullptr) {
        Event read_room(event);
        read_room.src_id = id();
        read_room.dst_id = group->group_number();
        read_room.pin = group->pin;
        read_room.event_data.literacy = literacy;
        sched_event(read_room);
        return;
    } else if (item->get_message().length() == 0) {
        message("There is no message to read!");
    } else {
        message(transcribe(item->get_message(),literacy,item->get_message_complexity()));
    }
}

void Actor::wear_command_event(const Event& event) {
    if (in_combat()) {
        message("You are fighting for your life!");
        return;
    }
    std::shared_ptr<Item> pick = find_item(*(event.key_words.get()));
    if (pick == nullptr) {
        message("You don't have that.");
    } else if (pick->wearable() == WearableSlots::Unwearable) {
        message("You can't wear that.");
    } else {
        if (pick->wearable() == WearableSlots::Body) {
            if (body != nullptr) {
                items.push_back(body);
            }
            body = pick;
        } else if (pick->wearable() == WearableSlots::Neck) {
            if (neck != nullptr) {
                items.push_back(neck);
            }
            neck = pick;
        }
        items.remove(pick);
        message("You put on "+pick->name().regular_name()+".");
        emit_stealthy(name.capitalized_name()+" puts on "+pick->name().regular_name()+".");
    }
}

void Actor::wield_command_event(const Event& event) {
   std::shared_ptr<Item> pick = find_item(*(event.key_words.get()));
    if (pick == nullptr) {
        if (secondary_hand != nullptr && secondary_hand->match_keywords(*(event.key_words.get())) > 0) {
            message("You move "+secondary_hand->name().regular_name()+" to your primary hand.");
            emit_stealthy(name.capitalized_name()+" moves "+secondary_hand->name().regular_name()+" to the primary hand.");
            std::swap(secondary_hand,primary_hand);
        } else {
            message("You don't have that.");
        }
    } else {
        if (primary_hand != nullptr) {
            message("You stow "+primary_hand->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" stows "+primary_hand->name().regular_name()+".");
            items.push_back(primary_hand);
        }
        items.remove(pick);
        primary_hand = pick;
        message("You wield "+primary_hand->name().regular_name()+".");
        emit_stealthy(name.capitalized_name()+" wields "+primary_hand->name().regular_name()+".");
    }
}

void Actor::hold_command_event(const Event& event) {
    std::shared_ptr<Item> pick = find_item(*(event.key_words.get()));
    if (pick == nullptr) {
        if (primary_hand != nullptr && primary_hand->match_keywords(*(event.key_words.get())) > 0) {
            message("You move "+primary_hand->name().regular_name()+" to your off hand.");
            emit_stealthy(name.capitalized_name()+" moves "+primary_hand->name().regular_name()+" to the off hand.");
            std::swap(secondary_hand,primary_hand);
        } else {
            message("You don't have that.");
        }
    } else {
        if (secondary_hand != nullptr) {
            message("You stow "+secondary_hand->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" stows "+secondary_hand->name().regular_name()+".");
            items.push_back(secondary_hand);
        }
        items.remove(pick);
        secondary_hand = pick;
        message("You hold "+secondary_hand->name().regular_name()+".");
        emit_stealthy(name.capitalized_name()+" holds "+secondary_hand->name().regular_name()+".");
    }
}

void Actor::trap_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    Dice die(1,20);
    int save = die(), dmg = event.event_data.trap.dmg_roll;
    switch(event.event_data.trap.attribute) {
        case Str:
            save += attribute_modifier(strength);
            break;
        case Dex:
            save += attribute_modifier(dexterity);
            break;
        case Con:
            save += attribute_modifier(constitution);
            break;
        case Int:
            save += attribute_modifier(intelligence);
            break;
        case Wis:
            save += attribute_modifier(wisdom);
            break;
        case Chr:
            save += attribute_modifier(charisma);
            break; 
        default:
            break;        
    }
    if (skills.find(event.event_data.trap.skill) != skills.end()) {
        save += skills[event.event_data.trap.skill];
    }
    if (save >= event.event_data.trap.save) {
        dmg /= 2;
    }
    damage += dmg;
    message(event.msg);
    auto trap_name = group->find_member(event.src_id)->get_name();
    emit(trap_name.capitalized_name()+" "+damage_adjective().second+" "+name.regular_name()+"!");
    message(trap_name.capitalized_name()+" "+damage_adjective().first+" you!");
    if (damage >= hit_points) {
        schedule_destroyed();
    }
} 

void Actor::lock_unlock_command_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    auto key = primary_hand;
    if (primary_hand == nullptr || primary_hand->get_key_code() == 0) {
        key = secondary_hand;
        if (secondary_hand == nullptr || secondary_hand->get_key_code() == 0) {
            message("You aren't holding a key.");
            return;
        }
    }
    Event result(event);
    result.type = Event::LOCK_UNLOCK;
    result.src_id = id();
    result.dst_id = group->group_number();
    result.pin = group->pin;
    result.item = key;
    result.stealthy = sneaking;
    sched_event(result);
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

bool Actor::check_skill(Skill skill, int difficulty, bool average) {
    int roll = use_skill(skill,average);
    return roll >= difficulty;
}

int Actor::use_skill(Skill skill, bool average) {
    Dice die(1,20);
    int result = (average) ? 10 : die();
    if (skill == Perception) {
        if (skills.find(Perception) != skills.end()) {
            result += skills[Perception];
        }
        result += attribute_modifier(wisdom);
    } else if (skill == Stealth) {
        if (skills.find(Stealth) != skills.end()) {
            result += skills[Stealth];
        }
        result += attribute_modifier(dexterity);
    } else if (skill == Swindle) {
        if (skills.find(Swindle) != skills.end()) {
            result += skills[Swindle];
        }
        result += attribute_modifier(charisma);
    } else if (skill == Literacy) {
        if (skills.find(Literacy) != skills.end()) {
            result += skills[Literacy];
        }
        result += attribute_modifier(intelligence);
    } else if (skill == Climbing) {
        if (skills.find(Literacy) != skills.end()) {
            result += skills[Literacy];
        }
        result += attribute_modifier(strength);
    }
    return std::max(0,result);
}

int Actor::use_item(std::shared_ptr<Item>& item) {
    Dice dice(1,20);
    int mod = 0;
    auto iter = skills.find(item->skill());
    if (iter != skills.end()) {
        mod = (*iter).second;
    }
    switch (item->modifier()) {
        case MonsterAttributes::Str:
            mod += attribute_modifier(strength);
            break;
        case MonsterAttributes::Dex:
            mod += attribute_modifier(dexterity);
            break;
        case MonsterAttributes::Con:
            mod += attribute_modifier(constitution);
            break;
        case MonsterAttributes::Int:
            mod += attribute_modifier(intelligence);
            break;
        case MonsterAttributes::Wis:
            mod += attribute_modifier(wisdom);
            break;
        case MonsterAttributes::Chr:
            mod += attribute_modifier(charisma);
            break;
        case MonsterAttributes::NoAttr:
            break;
    }
    return std::max(1,dice()+mod);
}

int Actor::melee_attack_delay(std::shared_ptr<Item>& item) {
    int delay = 750; // length of basic combat round
    if (item != nullptr) {
        // Weapon speed delay
        delay += item->weapon_info().speed*10;
    }
    // Dexterity bonus/penalty
    delay -= attribute_modifier(dexterity)*10;
    // Random factor
    delay += -10+rand()%20;
    return delay;
}

int Actor::total_ac() {
    int mod = 0;
    if (primary_hand != nullptr) {
        mod += primary_hand->ac_bonus_when_held();
    }
    if (secondary_hand != nullptr) {
        mod += secondary_hand->ac_bonus_when_held();
    }
    if (body != nullptr) {
        mod += body->ac_bonus_when_worn();
    }
    if (neck != nullptr) {
        mod += neck->ac_bonus_when_worn();
    }
    return armor_class + mod;
}

void Actor::account_for_cost(const std::shared_ptr<Item>& item, bool selling) {
    if (!group->is_shop()) {
        return;
    }
    if (selling) {
        int value = item->get_cost()/2;
        if (item->get_cost() > 0 && value == 0 && attribute_modifier(charisma) > 0) {
            value = 1;
        }
        if (items.empty()) {
            items.push_back(std::make_shared<Coins>(value));
        } else {
            Coins* c = dynamic_cast<Coins*>(items.back().get());
            if (c == nullptr) {
                items.push_back(std::make_shared<Coins>(value));
            } else {
                c->increment(value);
            }
        }
        consolidate_coins();
    } else {
        consolidate_coins();
        assert(!items.empty() || item->get_cost() == 0);
        if (items.empty()) {
            return;
        }
        Coins* c = dynamic_cast<Coins*>(items.back().get());
        if (c == nullptr) {
            assert(item->get_cost() == 0);
            return;
        }
        c->decrement(item->get_cost());
    }
}

int Actor::consolidate_coins() {
    auto coins = std::make_shared<Coins>(0);
    auto iter = items.begin();
    while (iter != items.end()) {
        Coins* c = dynamic_cast<Coins*>((*iter).get());
        if (c != nullptr) {
            coins->increment(c->get_cost());
            iter = items.erase(iter);
        } else {
            iter++;
        }
    }
    if (coins->get_cost() > 0) {
        items.push_back(coins);
        return items.back()->get_cost();
    }
    return 0;
}