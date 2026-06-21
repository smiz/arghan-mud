#include "trap.h"
#include <yaml-cpp/yaml.h>

Trap::Trap(Graph& graph, std::string file, int group_number):
Model(graph),
file(file),
delay(0),
periodic(false),
attr(NoAttr),
skill(NoSkill),
pending(false),
one_per_target(false),
effect(NoEffect),
intensity(0) {
    YAML::Node yaml = YAML::LoadFile(file.c_str());
    description = yaml["description"].as<std::string>();
    name = Name(yaml["name"].as<std::string>(),yaml["proper_name"].as<bool>());
    delay = yaml["delay"].as<int>();
    if (yaml["periodic"]) {
        periodic = yaml["periodic"].as<bool>();
    }
    dice = Dice(yaml["damage"].as<std::string>());
    save_number = yaml["save"].as<int>();
    if (yaml["skill"]) {
        skill = to_skill(yaml["skill"].as<std::string>());
    }
    if (yaml["attribute"]) {
        attr = to_monster_attribute(yaml["attribute"].as<std::string>());
    }
    if (yaml["one_per_target"]) {
        one_per_target = yaml["one_per_target"].as<bool>();
    }
    if (yaml["effect"]) {
        effect = to_effect(yaml["effect"]["name"].as<std::string>());
        intensity = yaml["effect"]["intensity"].as<int>();
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
    if (periodic && !group->zone_is_empty()) {
        pending = true;
        Event trap_event(event);
        sched_event(trap_event,delay);
    }
}

void Trap::join_prox_group_event(const Event& event) {
    if (pending) {
        return;
    }
    Event trap(Event::TRAP,id());
    if (one_per_target) {
        trap.dst_id = event.src_id;
    } else {
        trap.dst_id = ANY_ID;
    }
    trap.event_data.trap.effect = effect;
    trap.event_data.trap.intensity = intensity;
    trap.event_data.trap.dmg_roll = dice();
    trap.event_data.trap.attribute = attr;
    trap.event_data.trap.skill = skill;
    trap.event_data.trap.save = save_number;
    trap.msg = description;
    trap.pin = group->pin;
    pending = true;
    sched_event(trap,delay);
}