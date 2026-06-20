#include "model.h"

int ProximityGroupMember::next_id = NO_ID+1;
std::map<int,ProximityGroup*> Model::prox_map;

ProximityGroupMember::ProximityGroupMember():m_id(next_id++){}
std::map<int,int> ProximityGroup::zone_occupancy;

Model::Model(Graph& graph):
Atomic(),
ProximityGroupMember(),
group(nullptr),
graph(graph),
me(this),
departed(false) {
    graph.add_atomic(me);
    graph.connect(pin,me);
} 

Model::~Model() {
}

bool Model::filter(const Event& event) {
    if (event.src_id == id() && event.dst_id == ANY_ID_BUT_SRC) {
        return true;
    }
    if (event.dst_id != id() && event.dst_id >= 0) {
        return true;
    }
    if (event.exclude != nullptr && event.exclude->contains(id())) {
        return true;
    }
    return false;
}

void Model::leave_game() {
    if (departed) {
        return;
    }
    departed = true;
    pending.clear();
    if (group != nullptr) {
        graph.disconnect(group->pin,me);
    }
    graph.disconnect(pin,me);
    graph.remove_atomic(me);
    me = nullptr;
}

void Model::start_swindle_event(const Event& event) {
    if (filter(event)) {
        return;
    }
    Event result(Event::SEE1,id());
    result.dst_id = event.src_id;
    result.msg = "You can't swindle that!";
    result.pin = group->pin;
    sched_event(result);
}

std::shared_ptr<Item> Model::find_item(const KeyWordList& key_words) {
    std::shared_ptr<Item> result = nullptr;
    int best_score = 0;
    for (auto item: items) {
        int score = item->match_keywords(key_words);
        if (best_score < score) {
            result = item;
        }
    }
    return result;
}

void Model::receive_from(adevs::pin_t pin) {
    graph.connect(pin,me);
}

void Model::do_not_receive_from(adevs::pin_t pin) {
    graph.disconnect(pin,me);
}

void Model::delta_int() {
    tNow += ta();
    auto tOut = pending.front().first;
    do {
        if (pending.front().first != tOut) {
            break;
        }
        pending.pop_front();
    } while (!pending.empty());
}

void Model::delta_ext(Time e, const Bag& input) {
    if (departed) {
        return;
    }
    tNow += e;
    // Account for elapsed time
    for (auto& in_q: pending) {
        in_q.first -= e;
    }
    // Process new events
    for (auto x: input) {
        if (x.value.exclude != nullptr && x.value.exclude->contains(id())) {
            continue;
        }
        switch(x.value.type) {
            case Event::READ:
                read_event(x.value);
                break;
            case Event::JOIN_PROX_GROUP:
                join_prox_group_event(x.value);
                break;
            case Event::LEAVE_PROX_GROUP:
                leave_prox_group_event(x.value);
                break;
            case Event::SEE:
            case Event::SEE1:
                see_event(x.value);
                break;
            case Event::ENTER_MUD:
                enter_mud_event(x.value);
                break;
            case Event::LEAVE_MUD:
                leave_mud_event(x.value);
                break;
            case Event::MOVE:
                move_event(x.value);
                break;
            case Event::LOOK_COMMAND:
                look_command_event(x.value);
                break;
            case Event::GET_COMMAND:
                get_command_event(x.value);
                break;
            case Event::DROP_COMMAND:
                drop_command_event(x.value);
                break;
            case Event::LOOK:
                look_event(x.value);
                break;
            case Event::TRANSFER_ITEM:
                transfer_item_event(x.value);
                break;
            case Event::SAVE_MODEL:
                save_model_event(x.value);
                break;
            case Event::WIELD_COMMAND:
                wield_command_event(x.value);
                break;
            case Event::HOLD_COMMAND:
                hold_command_event(x.value);
                break;
            case Event::WEAR_COMMAND:
                wear_command_event(x.value);
                break;
            case Event::STOW_COMMAND:
                stow_command_event(x.value);
                break;
            case Event::KILL_COMMAND:
                kill_command_event(x.value);
                break;
            case Event::PENDING_ATTACK:
                pending_attack_event(x.value);
                break;
            case Event::MELEE_ATTACK:
                melee_attack_event(x.value);
                break;
            case Event::MELEE_RESULT:
                melee_result_event(x.value);
                break;
            case Event::DESTROYED:
                destroyed_event(x.value);
                break;
            case Event::RESET_ZONE:
                reset_zone_event(x.value);
                break;
            case Event::WANDER:
                wander_event(x.value);
                break;
            case Event::TRAP:
                trap_event(x.value);
                break;
            case Event::PRACTICE:
                practice_event(x.value);
                break;
            case Event::SNEAK_COMMAND:
                sneak_command_event(x.value);
                break;
            case Event::HEAR:
                hear_event(x.value);
                break;
            case Event::ROLL_PERIODIC_ATTRIBUTES:
                roll_periodic_attributes_event(x.value);
                break;
            case Event::SWINDLE:
                swindle_event(x.value);
                break;
            case Event::SWINDLE_COMMAND:
                swindle_command_event(x.value);
                break;
            case Event::SWINDLE_RESULT:
                swindle_result_event(x.value);
                break;
            case Event::START_SWINDLE:
                start_swindle_event(x.value);
                break;
            case Event::LOCK_UNLOCK_COMMAND:
                lock_unlock_command_event(x.value);
                break;
            case Event::LOCK_UNLOCK:
                lock_unlock_event(x.value);
                break;
            case Event::OPEN_CLOSE:
                open_close_event(x.value);
                break;
            case Event::USE_ITEM:
                use_item_event(x.value);
                break;
            default:
                break;
        }
    }
}

void Model::delta_conf(const Bag& input) {
    delta_int();
    delta_ext(adevs_zero<Time>(),input);
}

Time Model::ta() {
    if (pending.empty()) {
        return adevs_inf<Time>();
    }
    return pending.front().first;
}

void Model::output_func(Bag& output) {
    auto tOut = pending.front().first;
    for (auto y: pending) {
        if (y.first != tOut) {
            break;
        }
        output.push_back(adevs::PinValue(y.second.pin,y.second));
    }
}

void Model::cancel_event(Event::Type type) {
    auto iter = pending.begin();
    while (iter != pending.end()) {
        if ((*iter).second.type == type) {
            iter = pending.erase(iter);
        } else {
            iter++;
        }
    }
}

void Model::sched_event(Event& event, int time_to_event) {
    assert(time_to_event > 0);
    Time timestamp(time_to_event,event.type);
    std::pair<Time,Event> sched_event(timestamp,event);
    auto iter = pending.begin();
    while (iter != pending.end() && (*iter).first <= timestamp) {
        iter++;
    }
    pending.insert(iter,sched_event);
}

bool Model::in_combat() {
    for (auto event: pending) {
        switch(event.second.type) {
            case Event::MELEE_ATTACK:
            case Event::MELEE_RESULT:
                return true;
            default:
                break;
        }
    }
    return false;
}