#include "room.h"
#include "actor.h"
#include <yaml-cpp/yaml.h>

/// Must be in order of Direction enumeration in model.h!
static const std::string direction_key[6] = {
    "north", "south", "east", "west", "up", "down"
};

Room::Room(Graph& graph, std::string file):
Model(graph) {
    YAML::Node yaml = YAML::LoadFile(file.c_str());
    int prox_group_id = yaml["node"].as<int>();
    description = yaml["description"].as<std::string>();
    assert(prox_map.find(prox_group_id) == prox_map.end());
    ProximityGroup* group = new ProximityGroup(prox_group_id);
    prox_map[prox_group_id] = group;
    prox_groups.push_back(group);
    /// Will be first member in the group so that look with
    /// no argument queries the room.
    /// See ProximityGroup::find_best_match()
    group->add_member(this);
    receive_from(group->pin);
    /// Get our exits
    for (int i = 0; i < 6; i++) {
        std::string key = direction_key[i];
        if (yaml[key]) {
            direction_t dir {
                .id = yaml[key]["node"].as<int>(),
                .dir = Direction(i),
                .description = yaml[key]["description"].as<std::string>()
            };
            group->add_direction(dir);
        }
    }
    if (yaml["monsters"]) {
        Event enter;
        enter.type = Event::JOIN_PROX_GROUP;
        const YAML::Node& monster_list = yaml["monsters"];
        for (const auto& monster : monster_list) {
            Actor* actor = new Actor(graph,"monsters/"+monster.as<std::string>(),true,"",prox_group_id);
            std::cout << "monster " << monster.as<std::string>() << std::endl;
            enter.pin = actor->pin;
            enter.event_data.prox_group = prox_group_id;
            enter.dst_id = actor->id();
            enter.src_id = actor->id();
            sched_event(enter);
        }
    }
}

void Room::sched_see_event(int src_id) {
    Event see;
    see.type = Event::SEE;
    see.dst_id = src_id;
    see.src_id = id();
    see.pin = prox_groups.front()->pin;
    see.msg = description;
    direction_t exit_dir;
    for (int i = 0; i < int(EndOfDirectionEnum); i++) {
        exit_dir.dir = Direction(i);
        if (prox_groups.front()->find_direction(exit_dir)) {
            see.msg += "\n"+exit_dir.description;
        }
    }
    sched_event(see);
}

void Room::join_prox_group_event(const Event& event) {
    sched_see_event(event.src_id);
}

void Room::look_event(const Event& event) {
    if (event.dst_id != id() && event.dst_id != ANY_ID_BUT_SRC && event.dst_id != ANY_ID) {
        return;
    }
    sched_see_event(event.src_id);
}