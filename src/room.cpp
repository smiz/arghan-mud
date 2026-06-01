#include "room.h"
#include "actor.h"
#include "trap.h"
#include "maze.h"
#include "coins.h"
#include <yaml-cpp/yaml.h>

static const int reload_interval = 120000;

#define NO_ZONE -1

/// Must be in order of Direction enumeration in model.h!
static const std::string direction_key[6] = {
    "north", "south", "east", "west", "up", "down"
};

void Room::reload() {
    YAML::Node yaml = YAML::LoadFile(file.c_str());
    /// Get creatures in the room. Notice that a zone is empty only if every
    /// creature in it is dead or has left.
    if (yaml["monsters"]) {
        Event enter;
        enter.type = Event::JOIN_PROX_GROUP;
        const YAML::Node& monster_list = yaml["monsters"];
        for (const auto& monster : monster_list) {
            Actor* actor = new Actor(graph,"monsters/"+monster.as<std::string>(),true);
            if (initial_load) {
                std::cout << "monster " << monster.as<std::string>() << std::endl;
            }
            enter.pin = actor->pin;
            enter.event_data.prox_group = group->group_number();
            enter.dst_id = actor->id();
            enter.src_id = actor->id();
            sched_event(enter);
        }
    }
    /// Get items in the room
    items.clear();
    if (yaml["items"]) {
        Event enter;
        enter.type = Event::JOIN_PROX_GROUP;
        const YAML::Node& item_list = yaml["items"];
        for (const auto& item : item_list) {
            auto new_item = std::make_shared<Item>(item.as<std::string>());
            if (initial_load) {
                std::cout << "item " << item.as<std::string>() << std::endl;
            }
            items.push_back(new_item);
        }
    }
    if (yaml["coins"]) {
        int number = yaml["coins"].as<int>();
        items.push_back(std::make_shared<Coins>(number));
    }
}

Room::Room(Graph& graph, std::string file, int node_id):
Model(graph),
file(file),
graph(graph) {
    int zone = NO_ZONE;
    YAML::Node yaml = YAML::LoadFile(file.c_str());
    int prox_group_id = (node_id == -1) ? yaml["node"].as<int>() : node_id;
    description = yaml["description"].as<std::string>();
    short_description = yaml["short_description"].as<std::string>();
    if (short_description.back() != '\n') {
        short_description.push_back('\n');
    }
    if (description.back() != '\n') {
        description.push_back('\n');
    }
    if (yaml["zone"]) {
        zone = yaml["zone"].as<int>();
    }
    if (prox_map.find(prox_group_id) != prox_map.end()) {
        std::cout << "Duplicate node number " << prox_group_id << std::endl;
        exit(0);
    }
    prox_map[prox_group_id] = group = new ProximityGroup(prox_group_id,zone); 
    /// Will be first member in the group so that look with
    /// no argument queries the room.
    /// See ProximityGroup::find_best_match()
    group->add_member(this);
    receive_from(group->pin);
    /// Are we a shop?
    if (yaml["shop"]) {
        group->set_shop(yaml["shop"].as<bool>());
    }
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
            if (dir.id == MAZE_START_ROOM_NUMBER) {
                std::cout << "Loading the Maze" << std::endl;
                new Maze(graph,group->group_number());
            }
        }
    }
    /// Get traps in the room. 
    if (yaml["traps"]) {
        const YAML::Node& trap_list = yaml["traps"];
        for (const auto& trap : trap_list) {
            new Trap(graph,"traps/"+trap.as<std::string>(),group->group_number());
        }
    }
    reload();
    if (zone != NO_ZONE) {
        Event event(Event::RESET_ZONE,id());
        event.dst_id = id();
        event.pin = group->pin;
        sched_event(event,reload_interval);
    }
}

void Room::reset_zone_event(const Event& event) {
    Event reset(Event::RESET_ZONE,id());
    reset.dst_id = id();
    reset.pin = group->pin;
    if (group->zone_is_empty()) {
        reload();
    }
    sched_event(reset,reload_interval);
}

void Room::sched_see_event(int src_id, const KeyWordList& key_words, bool words_are_container, int16_t flags) {
    Event see;
    see.type = Event::SEE;
    see.dst_id = src_id;
    see.src_id = id();
    see.pin = group->pin;
    auto item = find_item(key_words);
    if (words_are_container) {
        if (item != nullptr) {
            if (item->container()) {
                see.msg = "Inside "+item->name().regular_name()+" you find ";
                auto contents = item->contents();
                if (contents.empty()) {
                    see.msg += "nothing.";
                } else {
                    see.msg += "\n";
                    for (auto item: contents) {
                        see.msg += item->name().capitalized_name()+"\n";
                    }
                }
            } else {
                see.msg = "There is nothing inside of that!";
            }
        } else {
            see.msg = "Look inside of what?";
        }
        sched_event(see);
        return;
    }
    // If any keywords match our items, then report the item
    // description
    if (item != nullptr) {
        see.msg = item->detail();
        see.type = Event::SEE1;
    } else if (key_words.empty()) {
        if (flags == SEE_SHORT) {
            see.msg = short_description;
        } else {
            see.msg = description;
        }
        direction_t exit_dir;
        for (int i = 0; i < int(EndOfDirectionEnum); i++) {
            exit_dir.dir = Direction(i);
            if (group->find_direction(exit_dir)) {
                see.msg += exit_dir.description+'\n';
            }
        }
        for (auto item: items) {
            std::string line;
            std::ostringstream sout(line);
            sout << item->description();
            if (group->is_shop()) {
                sout << " (" << item->get_cost() << " coins)";
            }
            sout << std::endl;
            see.msg += sout.str();
        }
    } else {
        see.type = Event::SEE1;
        see.msg = "Look at what?";
    }
    see.flags = flags;
    sched_event(see);
}

void Room::join_prox_group_event(const Event& event) {
    KeyWordList empty;
    sched_see_event(event.src_id,empty,false,SEE_LONG);
    sched_see_event(event.src_id,empty,false,SEE_SHORT);
}

void Room::look_event(const Event& event) {
    if (event.dst_id != id() && event.dst_id != ANY_ID_BUT_SRC && event.dst_id != ANY_ID) {
        return;
    }
    sched_see_event(event.src_id, *(event.key_words.get()),event.event_data.transfer.first_keyword_is_container);
}

void Room::transfer_item_event(const Event& event) {
    bool missed_container = false;
    if (event.dst_id != id()) {
        return;
    }
    ProximityGroupMember* target = nullptr;
    for (auto rx_group: group->members()) {
        if (rx_group->id() == event.src_id) {
            target = rx_group;
            break;
        }
    }
    if (target == nullptr) {
        return;
    }
    std::shared_ptr<Item> container;
    Event transfer(event);
    Event see(Event::SEE1,id());
    see.dst_id = event.src_id;
    see.pin = group->pin;
    /// Look for a container to transfer from or to
    if (event.event_data.transfer.first_keyword_is_container) {
        KeyWordList container_words;
        container_words.push_back(event.key_words->front());
        container = find_item(container_words);
        if (container == nullptr) {
            missed_container = true;
            see.msg = "You don't see that.";
            if (event.event_data.transfer.src_to_dst) {
                see.msg += " You drop it on the ground.";
                sched_event(see);
            } else {
                sched_event(see);
                return;
            }
        } else if (!container->container()) {
            missed_container = true;
            see.msg = container->name().capitalized_name() + " can't hold anything!";
            if (event.event_data.transfer.src_to_dst) {
                see.msg += " You drop it on the ground.";
                sched_event(see);
            } else {
                sched_event(see);
                return;
            }
            // We'll drop it on the ground
            container = nullptr;
        }
    }
    /// Receive an item
    if (event.event_data.transfer.src_to_dst) {
        if (container != nullptr) {
            container->add_item(event.item);
            see.msg = target->get_name().capitalized_name()+
                " puts " + event.item->name().regular_name()+" into "+container->name().regular_name()+".";
            see.src_id = event.src_id;
            see.dst_id = ANY_ID_BUT_SRC;
            sched_event(see);
            see.msg = "You put " + event.item->name().regular_name()+" into "+container->name().regular_name()+".";
            see.dst_id = event.src_id;
            sched_event(see);
        } else {
            items.push_back(event.item);
            see.msg = target->get_name().capitalized_name()+
                " drops " + event.item->name().regular_name()+".";
            see.src_id = event.src_id;
            see.dst_id = ANY_ID_BUT_SRC;
            sched_event(see);
            if (!missed_container) {
                see.msg = "You drop " + event.item->name().regular_name()+".";
                see.dst_id = event.src_id;
                sched_event(see);
            }
        }
    } else { // Give up an item if we have it
        transfer.src_id = id();
        transfer.dst_id = event.src_id;
        transfer.event_data.transfer.src_to_dst = true;
        if (container == nullptr) {
            transfer.item = find_item(*(event.key_words.get()));
        } else {
            KeyWordList obj_words(*(event.key_words.get()));
            obj_words.pop_front();
            transfer.item = container->remove_item(obj_words);
        }
        if (transfer.item == nullptr) {
            see.pin = target->pin;
            see.msg = "That object isn't here.";
            sched_event(see);
        } else {
            if (group->is_shop() && transfer.event_data.transfer.coins_in_purse < transfer.item->get_cost()) {
                see.src_id = id();
                see.dst_id = event.src_id;
                see.pin = target->pin;
                see.msg = "You can't afford that!";
                sched_event(see);
                return;
            }
            if (container == nullptr) {
                items.remove(transfer.item);
                see.msg = target->get_name().capitalized_name()+
                    " picks up " + transfer.item->name().regular_name()+".";
            } else {
                see.msg = target->get_name().capitalized_name()+
                    " gets " + transfer.item->name().regular_name()+" from " + container->name().regular_name()+".";
            }
            see.src_id = event.src_id;
            see.dst_id = ANY_ID_BUT_SRC;
            sched_event(see);
            transfer.pin = target->pin;
            sched_event(transfer);
            see.dst_id = event.src_id;
            see.pin = target->pin;
            if (container == nullptr) {
                see.msg = "You pick it up.";
            } else {
                see.msg = "You get it from "+container->name().regular_name()+".";
            }
            sched_event(see);
        }
    }
}

void Room::destroyed_event(const Event& event) {
    assert(event.dst_id == group->group_number());
    if (event.item != nullptr) {
        items.push_back(event.item);
    }
}