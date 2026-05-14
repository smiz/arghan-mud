#include "dice.h"
#include <random>

static std::random_device r;
static std::default_random_engine gen(r());

Dice::Dice(int number, int die_type, int modifier):
number(number),
die(1,die_type),
modifier(modifier) {

}

int Dice::operator()() {
    int total = die(gen);
    for (int i = 1; i < number; i++) {
        total += die(gen);
    }
    return total+modifier;
}