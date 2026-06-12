#include "trap.h"
#include <yaml-cpp/yaml.h>

Trap::Trap(Graph& graph, std::string file, int group_number):
Model(graph),
file(file),
attr(NoAttr),
skill(NoSkill),
pending(false) {
    YAML::Node yaml = YAML::LoadFile(file.c_str());
    description = yaml["description"].as<std::string>();
    name = Name(yaml["name"].as<std::string>(),yaml["proper_name"].as<bool>());
    delay = yaml["delay"].as<int>();
    dice = Dice(yaml["damage"].as<std::string>());
    save_number = yaml["save"].as<int>();
    if (yaml["skill"]) {
        skill = to_skill(yaml["skill"].as<std::string>());
    }
    if (yaml["attribute"]) {
        attr = to_monster_attribute(yaml["attribute"].as<std::string>());
    }
    group = prox_map[group_number];
    group->add_member(this);
    receive_from(group->pin);
}

void Trap::reset_zone_event(const Event& event) {
    if (!group->zone_is_empty()) {
        return;
    }
    group->remove_member(this);
    do_not_receive_from(group->pin);
    Model::leave_game();
}

void Trap::trap_event(const Event& event) {
    if (event.src_id == id()) {
        pending = false;
    }
}

void Trap::join_prox_group_event(const Event& event) {
    if (pending) {
        return;
    }
    Event trap(Event::TRAP,id());
    trap.dst_id = ANY_ID;
    trap.event_data.trap.dmg_roll = dice();
    trap.event_data.trap.attribute = attr;
    trap.event_data.trap.skill = skill;
    trap.event_data.trap.save = save_number;
    trap.msg = description;
    trap.pin = group->pin;
    pending = true;
    sched_event(trap,delay);
}