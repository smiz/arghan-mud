#include "utils.h"
#include "dice.h"

std::string transcribe(std::string msg, int literacy, int difficulty) {
    Dice die(1,20);
    std::string result;
    for (auto c: msg) {
        if (c >= 32 && c <= 126 && literacy < difficulty+die()) {
            c = 32+rand()%(126-32);
        }
        result.push_back(c);
    }
    return result;
}