#include "dice.h"
#include <random>
#include <cassert>
#include <iostream>
#include <sstream>

static std::random_device r;
static std::default_random_engine gen(r());

Dice::Dice():
number(0),
die_sides(1),
modifier(0),
die(0,1) {

}

Dice::Dice(int number, int die_type, int modifier):
number(number),
die_sides(die_type),
modifier(modifier),
die(1,die_type) {

}

std::string Dice::str() {
    std::ostringstream sout;
    sout << number << "d" << die_sides;
    if (modifier > 0) {
        sout << "+" << modifier;
    } else if (modifier < 0) {
        sout << "-" << modifier;
    }
    return sout.str();
}

Dice::Dice(std::string die_type) {
    die_sides = 0;
    modifier = 0;
    number = 0;
    unsigned i = 0;
    char mod_char = '+';
    std::string token;
    for (i = 0; i < die_type.length(); i++) {
        if (isdigit(die_type[i])) {
            token += die_type[i];
        } else {
            number = atoi(token.c_str());
            token.clear();
            break;
        }
    }
    if (die_type[i] != 'd') {
        throw "bad die description "+die_type;
    }
    i++;
    for(; i < die_type.length(); i++) {
        if (isdigit(die_type[i])) {
            token += die_type[i];
        } else {
            die_sides = atoi(token.c_str());
            token.clear();
            break;
        }
    }
    if (i == die_type.length()) {
        if (token.length() == 0) {
            throw "bad die type "+die_type;
        }
        die_sides = atoi(token.c_str());
    }
    if (die_sides <= 0) {
        throw "bad die sides "+die_type;
    }
    if (die_type[i] != '+' && die_type[i] != '-' && i != die_type.length()) {
        throw "bad die modifier "+die_type;
    }
    if (i < die_type.length()) {
        mod_char = die_type[i];
        i++;
        if (die_type.length() == i) {
            throw "bad die modifier "+die_type;
        }
        for (; i < die_type.length(); i++) {
            if (isdigit(die_type[i])) {
                token += die_type[i];
            } else {
                modifier = atoi(token.c_str());
                if (mod_char == '-') {
                    modifier *= -1;
                }
                token.clear();
                break;
            }
        }
        if (token.length() > 0) {
            modifier = atoi(token.c_str());
            if (mod_char == '-') {
                modifier *= -1;
            }
        }
    }
    die = std::uniform_int_distribution<>(1,die_sides);
}

int Dice::operator()() {
    int total = 0;
    for (int i = 0; i < number; i++) {
        total += die(gen);
    }
    return std::max(0,total+modifier);
}