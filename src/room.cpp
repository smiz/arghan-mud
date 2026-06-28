#include "room.h"
#include "actor.h"
#include "trap.h"
#include "maze.h"
#include "coins.h"
#include "utils.h"
#include <cassert>
#include <yaml-cpp/yaml.h>

static const int reload_interval = 120000;

#define NO_ZONE -1

int Room::max_room_number = 0;

static Direction opposite_direction(Direction d) {
    if (d == North) {
        return South;
    }
    if (d == South) {
        return North;
    }
    if (d == East) {
        return West;
    } 
    if (d == West) {
        return East;
    }
    if (d == Up) {
        return Down;
    }
    return Up;
}

/// Must be in order of Direction enumeration in types.h!
static const std::string direction_key[int(EndOfDirectionEnum)] = {
    "north", "south", "east", "west", "up", "down", "flee"
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
    /// Get traps in the room. 
    if (yaml["traps"]) {
        const YAML::Node& trap_list = yaml["traps"];
        for (const auto& trap : trap_list) {
            new Trap(graph,"traps/"+trap.as<std::string>(),group->group_number());
        }
    }
    /// Get our doors
    for (int i = 0; i < int(EndOfDirectionEnum); i++) {
        doors[i] = nullptr;
        std::string key = direction_key[i];
        if (yaml[key]) {
            if (yaml[key]["door"]) {
                doors[i] = std::make_shared<Item>(yaml[key]["door"].as<std::string>());
                group->block_direction(Direction(i),doors[i]->is_locked());
            }
        }
    }
}

std::shared_ptr<Item> Room::find_item_or_door(const KeyWordList& key_words) {
    int score = 0;
    auto result = Model::find_item(key_words);
    if (result != nullptr) {
        score = result->match_keywords(key_words);
    }
    for (int i = 0; i < int(EndOfDirectionEnum); i++) {
        if (doors[i] != nullptr) {
            int door_score = doors[i]->match_keywords(key_words);
            if (door_score > score) {
                score = door_score;
                result = doors[i];
            }
        }
    }
    return result;
}

int Room::next_free_room_number() {
    return ++max_room_number;
}

Room::Room(Graph& graph, std::string file, int node_id):
Model(graph),
file(file),
graph(graph),
initial_load(true) {
    for (int i = 0; i < int(EndOfDirectionEnum); i++) {
        doors[i] = nullptr;
    }
    int zone = NO_ZONE;
    YAML::Node yaml = YAML::LoadFile(file.c_str());
    int prox_group_id = (node_id == -1) ? yaml["node"].as<int>() : node_id;
    max_room_number = std::max(prox_group_id,max_room_number);
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
            direction_t dir;
            dir.id = yaml[key]["node"].as<int>();
            dir.dir = Direction(i);
            dir.description = yaml[key]["description"].as<std::string>();
            if (dir.id == MAZE_START_ROOM_NUMBER) {
                std::cout << "Loading the Maze" << std::endl;
                new Maze(graph,group->group_number());
            }
            if (yaml[key]["skill"]) {
                dir.skill = to_skill(yaml[key]["skill"].as<std::string>());
                dir.difficulty = yaml[key]["difficulty"].as<int>();
                if (yaml[key]["damage"]) {
                    dir.dmg = std::make_shared<Dice>(yaml[key]["damage"].as<std::string>());
                } else {
                    dir.dmg = nullptr;
                }
                dir.go_on_fail = yaml[key]["go_on_fail"].as<bool>();
                dir.success_msg1 = yaml[key]["success1"].as<std::string>();
                dir.fail_msg1 = yaml[key]["fail1"].as<std::string>();
                dir.success_msg2 = yaml[key]["success2"].as<std::string>();
                dir.fail_msg2 = yaml[key]["fail2"].as<std::string>();
            }
            group->add_direction(dir);
        }
    }
    reload();
    initial_load = false;
    if (zone != NO_ZONE) {
        Event event(Event::RESET_ZONE,id());
        event.dst_id = ANY_ID;
        event.pin = group->pin;
        sched_event(event,reload_interval);
    }
}

void Room::read_event(const Event& event) {
    Event result(Event::SEE1,id());
    result.dst_id = event.src_id;
    result.pin = group->pin;
    auto item = find_item_or_door(*(event.key_words.get()));
    if (item == nullptr) {
        result.msg = "You don't see that.";
    } else if (!item->is_heavy()) {
        result.msg = "You'll have to pick it up first.";
    } else if (item->get_message().length() == 0) {
        result.msg = "Nothing is written there.";
    } else {
        result.msg = transcribe(item->get_message(),event.event_data.literacy,item->get_message_complexity());
    }
    sched_event(result);
}

void Room::reset_zone_event(const Event& event) {
    Event reset(event);
    if (group->zone_is_empty()) {
        reload();
    }
    sched_event(reset,reload_interval);
}

void Room::lock_unlock_event(const Event& event) {
    Event result(Event::SEE1,id());
    bool lock_state = true;
    result.event_data.subject_id = result.dst_id = event.src_id;
    result.pin = group->pin;
    auto item = find_item_or_door(*(event.key_words.get()));
    if (item == nullptr) {
        result.msg = "You don't see that.";
        sched_event(result);
        return;
    }
    if (!item->is_locked() && !event.event_data.lock) {
        result.msg = "It isn't locked.";
        sched_event(result);
        return;
    }
    if (item->is_locked() && event.event_data.lock) {
        result.msg = "It is already locked.";
        sched_event(result);
        return;
    }
    if (item->get_lock_code() == event.item->get_key_code()) {
        if (test_open_close(item)) {
            lock_state = !item->is_locked();
        } else {
            item->toggle_lock();
            lock_state = item->is_locked();
        }
    } else {
        result.msg = "The key doesn't fit.";
        sched_event(result);
        return;
    }
    if (lock_state) {
        result.msg = "You lock "+item->name().regular_name()+".";
        sched_event(result);
        result.msg = group->find_member(event.src_id)->get_name().capitalized_name()+ " locks " + item->name().regular_name()+".";
    } else {
        result.msg = "You unlock "+item->name().regular_name()+".";
        sched_event(result);
        result.msg = group->find_member(event.src_id)->get_name().capitalized_name()+ " unlocks " + item->name().regular_name()+".";
    }
    result.exclude = std::make_shared<std::set<int>>();
    result.exclude->insert(event.src_id);
    result.dst_id = ANY_ID_BUT_SRC;
    result.stealthy = event.stealthy;
    sched_event(result);
}

void Room::open_close_event(const Event& event) {
    assert(doors[event.event_data.dir] != nullptr);
    Event see(Event::SEE1,id());
    see.dst_id = ANY_ID_BUT_SRC;
    see.pin = group->pin;
    auto door = doors[event.event_data.dir];
    door->toggle_lock();
    group->block_direction(event.event_data.dir,door->is_locked());
    if (door->is_locked()) {
        see.msg = door->name().capitalized_name()+" is closed.";
    } else {
        see.msg = door->name().capitalized_name()+" is open.";
    }
    sched_event(see);
}

bool Room::test_open_close(std::shared_ptr<Item> item) {
    Event event(Event::OPEN_CLOSE,id());
    for (int i = 0; i < int(EndOfDirectionEnum); i++) {
        if (item == doors[i]) {
            direction_t dir;
            dir.dir = Direction(i);
            group->find_direction(dir);
            event.pin = group->pin;
            event.event_data.dir = dir.dir;
            event.dst_id = ANY_ID;
            sched_event(event);
            event.pin = prox_map[dir.id]->pin;
            event.event_data.dir = opposite_direction(dir.dir);
            sched_event(event);
            return true;
        }
    }
    return false;
}

void Room::sched_see_event(int src_id, const KeyWordList& key_words, bool words_are_container, int16_t flags) {
    Event see;
    see.type = Event::SEE;
    see.event_data.subject_id = -1;
    see.dst_id = src_id;
    see.src_id = id();
    see.pin = group->pin;
    auto item = find_item_or_door(key_words);
    if (words_are_container) {
        if (item != nullptr) {
            if (item->container()) {
                if (!item->is_locked()) {
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
                    see.msg = item->name().capitalized_name()+" is locked!";
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
            group->find_direction(exit_dir);
            if (doors[i] != nullptr) {
                if (doors[i] != nullptr) {
                    if (doors[i]->is_locked()) {
                        see.msg += doors[i]->description()+'\n';
                    } else {
                        see.msg += exit_dir.description+'\n';
                    }
                } else {
                    see.msg += exit_dir.description+'\n';
                }
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
    assert(event.key_words != nullptr);
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
    see.event_data.subject_id = see.dst_id = event.src_id;
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
        } else if (container->is_locked()) {
            missed_container = true;
            see.msg = container->name().capitalized_name() + " is locked.";
            if (event.event_data.transfer.src_to_dst) {
                see.msg += " You drop it on the ground.";
                sched_event(see);
            } else {
                sched_event(see);
                return;
            }
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
            see.stealthy = event.stealthy;
            sched_event(see);
            see.msg = "You put " + event.item->name().regular_name()+" into "+container->name().regular_name()+".";
            see.dst_id = event.src_id;
            see.stealthy = 0;
            sched_event(see);
        } else {
            items.push_back(event.item);
            see.msg = target->get_name().capitalized_name()+
                " drops " + event.item->name().regular_name()+".";
            see.src_id = event.src_id;
            see.dst_id = ANY_ID_BUT_SRC;
            see.stealthy = event.stealthy;
            sched_event(see);
            if (!missed_container) {
                see.msg = "You drop " + event.item->name().regular_name()+".";
                see.dst_id = event.src_id;
                see.stealthy = 0;
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
        } else if (transfer.item->is_heavy()) {
            see.pin = target->pin;
            see.msg = "That object is too heavy to lift.";
            // Put it back in the container if that is where it came from
            if (container != nullptr) {
                container->add_item(transfer.item);
            }
            sched_event(see);
        } else {
            if (group->is_shop() && transfer.event_data.transfer.coins_in_purse < transfer.item->get_cost()) {
                see.src_id = id();
                see.dst_id = event.src_id;
                see.pin = target->pin;
                see.msg = "You can't afford that!";
                // Put it back in the container if that is where it came from
                if (container != nullptr) {
                    container->add_item(transfer.item);
                }
                sched_event(see);
                return;
            }
            if (container == nullptr) {
                items.remove(transfer.item);
                see.stealthy = event.stealthy;
                see.msg = target->get_name().capitalized_name()+
                    " picks up " + transfer.item->name().regular_name()+".";
            } else {
                see.stealthy = event.stealthy;
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
            see.stealthy = 0;
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