#include "actor.h"
#include "dice.h"
#include "corpse.h"
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
wanders(0),
aggressive(false),
sneaking(0),
perceiving(0),
level(0),
xp_to_go(0),
free_skill_slots(0),
fd(-1),
exit_node_id(START_GROUP),
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
        if (yaml["aggressive"]) {
            aggressive = yaml["aggressive"].as<bool>();
        }
        if (yaml["wear_body"]) {
            body = std::make_shared<Item>(yaml["wear_body"].as<std::string>());
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
    } else {
        init(stats);
        save();
    }
    perceiving = use_skill(Perception);
    Event periodic(Event::ROLL_PERIODIC_ATTRIBUTES,id());
    periodic.dst_id = id();
    periodic.pin = pin;
    sched_event(periodic);
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
        xp_to_go = level*10;
        hit_points += std::max(1,hp_die()+attribute_modifier(constitution));
        message("You gained a level!");
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
    description = name.capitalized_name()+" is here.";
    detail = name.capitalized_name()+" is pretty good lookin'!";
    key_words.push_back(name.get_name());
    key_words.push_back(name.lower_case());
}

static void build_inventory(std::shared_ptr<Item>& item, std::vector<std::string>& item_files) {
    /// Don't include transient items (specifically, corpses)
    if (!item->filename().empty()) {
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

void Actor::roll_periodic_attributes_event(const Event& event) {
    Event again(event);
    perceiving = use_skill(Perception);
    sched_event(again,60000);
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
    enter.pin = prox_map[exit_node_id]->pin;
    enter.dst_id = ANY_ID_BUT_SRC;
    enter.msg = name.capitalized_name()+" returns from the Real World!";
    sched_event(enter);
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
    if (event.event_data.melee.atk_roll > total_ac()) {
        std::string wpn_name = "";
        std::string adj1, adj2;
        if (event.item != nullptr) {
            wpn_name = event.item->name().regular_name();
        }
        damage += event.event_data.melee.dmg_roll;
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
    sched_event(result);
    // If we are not in combat already (we were surprised for example)
    if (!in_combat()) {
        schedule_attack(event.src_id,false);
    }
    if (damage >= hit_points) {
        schedule_destroyed();
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
    sched_event(event); 
}

void Actor::destroyed_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    group->remove_member(this);
    do_not_receive_from(group->pin);
    if (pc) {
        sneaking = 0;
        group = prox_map[START_GROUP];
        receive_from(group->pin);
        group->add_member(this);
        emit(name.capitalized_name()+" suddenly appears!");
        message("Ashes to ashes...dust to dust...and then remade.");
        message("Try looking around.");
        exit_node_id = START_GROUP;
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
        group->find_member(target_id)->hidden() > perceiving) {
        message("You don't see anything like that.");
        return;
    } else {
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
    Event attack(Event::MELEE_ATTACK,id());
    attack.dst_id = target_id;
    attack.pin = group->pin;
    if (primary_hand != nullptr) {
        attack.event_data.melee.dmg_roll =
            std::max(1,primary_hand->weapon_info().dmg_die()+attribute_modifier(strength));
            attack.event_data.melee.atk_roll = use_item(primary_hand);
    } else {
        attack.event_data.melee.dmg_roll = std::max(1,attribute_modifier(strength));
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
    }
}

void Actor::pending_attack_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    if (event.stealthy == 0 || perceiving >= event.stealthy) {
        schedule_attack(event.src_id,false);
    }
}

void Actor::leave_mud_event(const Event& event) {
    exit_node_id = group->group_number();
    Event leave(Event::LEAVE_PROX_GROUP,id());
    leave.event_data.prox_group = group->group_number();
    leave.pin = group->pin;
    leave.dst_id = id();
    sched_event(leave);
    emit_stealthy(name.capitalized_name()+" mutters something about the Real World and vanishes!");
    sched_event(leave);
}

void Actor::join_prox_group_event(const Event& event) {
    if (event.dst_id == id()) {
        group = prox_map[event.event_data.prox_group];
        group->add_member(this);
        receive_from(group->pin);
    } else {
        if (!in_combat() && (aggressive || hates.contains(event.src_id))
            && (event.stealthy == 0 || event.stealthy < perceiving)) {
            emit_stealthy(description+
                "\n"+name.capitalized_name()+
                " looks murderously at you.",event.src_id);
            schedule_attack(event.src_id,true);
        } else {
            emit_stealthy(description);
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
    dir.dir = group->random_exit();
    group->find_direction(dir);
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
        if (Dice(1,20)()+attribute_modifier(dexterity) < 10) {
            message("You couldn't escape!");
            return;
        } else {
            move_dir.dir = group->random_exit();
        }
    }
    if (!group->find_direction(move_dir)) {
        message("You can't go in that direction.");
        return;
    }
    change_prox_groups(move_dir.id);
    Event see(Event::SEE,id());
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
        if (event.stealthy == 0 || perceiving > event.stealthy) {
            if (short_descriptions && event.flags & SEE_SHORT) {
                message(event.msg);
            } else if (!short_descriptions && event.flags & SEE_LONG) {
                message(event.msg);
            }
        }
    }
}

void Actor::transfer_item_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    assert(event.event_data.transfer.src_to_dst);
    gain_xp(event.item->claim_xp());
    items.push_back(event.item);
    save();
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
    get.src_id = id();
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
    drop.item = find_item(*(event.key_words.get()));
    // Not in our pack
    if (drop.item != nullptr) {
        items.remove(drop.item);
    } else {
        int best_score = 0, score = 0;
        // Is it equipped?
        if (primary_hand != nullptr) {
            score = primary_hand->match_keywords(*(event.key_words.get()));
            if (score > best_score) {
                best_score = score;
                drop.item = primary_hand;
            }
        }
        if (secondary_hand != nullptr) {
            score = secondary_hand->match_keywords(*(event.key_words.get()));
            if (score > best_score) {
                best_score = score;
                drop.item = secondary_hand;
            }
        }
        if (body != nullptr) {
            score = body->match_keywords(*(event.key_words.get()));
            if (score > best_score) {
                best_score = score;
                drop.item = body;
            }
        }
        if (drop.item == primary_hand) {
            primary_hand = nullptr;
        } else if (drop.item == secondary_hand) {
            secondary_hand = nullptr;
        } else if (drop.item == body) {
            body = nullptr;
        }
    }
    if (drop.item != nullptr) {
        save();
        sched_event(drop);
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
        /// Look at or in something specific
        look.dst_id = group->find_best_match(*(event.key_words));
        if (group->find_member(look.dst_id)->hidden() > perceiving) {
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
    see.dst_id = event.src_id;
    if (event.dst_id == id()) {
        see.msg = detail; 
    } else {
        see.msg = description;
    }
    if (see.msg.back() != '\n') {
            see.msg += '\n';
    }
    if (primary_hand != nullptr) {
        see.msg += name.capitalized_name()+" wields "+primary_hand->name().regular_name()+".\n";
    }
    if (secondary_hand != nullptr) {
        see.msg += name.capitalized_name()+" holds "+secondary_hand->name().regular_name()+".\n";
    }
    if (body != nullptr) {
        see.msg += name.capitalized_name()+" wears "+body->name().regular_name()+".\n";
    }
    if (!items.empty()) {
        see.msg += name.capitalized_name()+" is carrying:\n";
        for (auto item: items) {
            see.msg += item->name().capitalized_name()+"\n";
        }
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
        } else {
            message("You remove "+body->name().regular_name()+".");
            emit_stealthy(name.capitalized_name()+" removes "+body->name().regular_name()+".");
            body = nullptr;
        }
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
        if (body != nullptr) {
            items.push_back(body);
        }
        items.remove(pick);
        body = pick;
        message("You put on "+body->name().regular_name()+".");
        emit_stealthy(name.capitalized_name()+" puts on "+body->name().regular_name()+".");
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

int Actor::use_skill(Skill skill) {
    Dice die(1,20);
    int result = die();
    if (skill == Perception) {
        if (skills.find(Perception) != skills.end()) {
            result += skills[Perception];
        }
        result += attribute_modifier(wisdom);
    } else if (skill == Stealth) {
        if (skills.find(Stealth) != skills.end()) {
            result += skills[Perception];
        }
        result += attribute_modifier(dexterity);
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
    int delay = 500; // Half second is basic combat round
    if (item != nullptr) {
        // Weapon speed delay
        delay += item->weapon_info().speed*5;
    }
    // Dexterity bonus/penalty
    delay -= attribute_modifier(dexterity)*5;
    // Random factor
    delay += -10+rand()%20;
    // Fastet is 1/10 of a second
    delay = std::max(delay,100);
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
    return armor_class + mod;
}

