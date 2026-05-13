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
    group = new ProximityGroup(prox_group_id);
    prox_map[prox_group_id] = group;
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
    /// Get creatures in the room
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
    /// Get items in the room
    if (yaml["items"]) {
        Event enter;
        enter.type = Event::JOIN_PROX_GROUP;
        const YAML::Node& item_list = yaml["items"];
        for (const auto& item : item_list) {
            auto new_item = std::make_shared<Item>(item.as<std::string>());
            std::cout << "item " << item.as<std::string>() << std::endl;
            items.push_back(new_item);
        }
    }
}

void Room::sched_see_event(int src_id, const KeyWordList& key_words) {
    Event see;
    see.type = Event::SEE;
    see.dst_id = src_id;
    see.src_id = id();
    see.pin = group->pin;
    // If any keywords match our items, then report the item
    // description
    auto item = find_item(key_words);
    if (item != nullptr) {
        see.msg = item->detail();
        see.type = Event::SEE1;
    } else {
        if (key_words.empty()) {
            see.msg = description;
            direction_t exit_dir;
            for (int i = 0; i < int(EndOfDirectionEnum); i++) {
                exit_dir.dir = Direction(i);
                if (group->find_direction(exit_dir)) {
                    see.msg += "\n"+exit_dir.description;
                }
            }
            for (auto item: items) {
                see.msg += "\n"+item->description();
            }
        } else {
            see.msg = "Look at what?";
        }
    }
    sched_event(see);
}

void Room::join_prox_group_event(const Event& event) {
    KeyWordList empty;
    sched_see_event(event.src_id,empty);
}

void Room::look_event(const Event& event) {
    if (event.dst_id != id() && event.dst_id != ANY_ID_BUT_SRC && event.dst_id != ANY_ID) {
        return;
    }
    sched_see_event(event.src_id, *(event.key_words.get()));
}

void Room::transfer_item_event(const Event& event) {
    if (event.dst_id != id()) {
        return;
    }
    Event transfer(event);
    Event see;
    see.type = Event::SEE1;
    see.dst_id = event.src_id;
    see.src_id = id();
    see.pin = group->pin;
    /// Receive an item
    if (event.event_data.transfer_src_to_dst) {
        items.push_back(event.item);
    } else { // Give up an item if we have it
        transfer.item = find_item(*(event.key_words.get()));
        ProximityGroupMember* target = nullptr;
        transfer.src_id = id();
        transfer.dst_id = event.src_id;
        transfer.event_data.transfer_src_to_dst = true;
        /// Send it directly to the model that should receive the item
        for (auto rx_group: group->members()) {
            if (rx_group->id() == event.src_id) {
                target = rx_group;
                break;
            }
        }
        if (target == nullptr) {
            return;
        }
        if (transfer.item == nullptr) {
            see.pin = target->pin;
            see.msg = "That object isn't here.";
            sched_event(see);
        } else {
            items.remove(transfer.item);
            see.msg = target->get_name().capitalized_name()+
                " picks up " + transfer.item->name().regular_name()+".";
            see.src_id = event.src_id;
            see.dst_id = ANY_ID_BUT_SRC;
            sched_event(see);
            transfer.pin = target->pin;
            sched_event(transfer);
            see.dst_id = event.src_id;
            see.pin = target->pin;
            see.msg = "You pick it up.";
            sched_event(see);
        }
    }
}