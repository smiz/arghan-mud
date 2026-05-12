#include "model.h"

int ProximityGroupMember::next_id = NO_ID+1;
std::map<int,ProximityGroup*> Model::prox_map;

ProximityGroupMember::ProximityGroupMember():m_id(next_id++){}

Model::Model(Graph& graph):
Atomic(),
ProximityGroupMember(),
graph(graph),
me(this),
departed(false) {
    graph.add_atomic(me);
    graph.connect(pin,me);
} 

Model::~Model() {

}

void Model::leave_game() {
    departed = true;
    graph.disconnect(pin,me);
    graph.remove_atomic(me);
    pending.clear();
    me.reset();
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
    for (auto in_q: pending) {
        in_q.first -= e;
    }
    // Process new events
    for (auto x: input) {
        switch(x.value.type) {
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

void Model::sched_event(Event& event, int time_to_event) {
    assert(time_to_event > 0);
    Time timestamp(time_to_event,event.type);
    std::pair<Time,Event> sched_event(timestamp,event);
    auto iter = pending.begin();
    while (iter != pending.end() && (*iter).first < timestamp) {
        iter++;
    }
    pending.insert(iter,sched_event);
}