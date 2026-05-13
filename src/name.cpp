#include "name.h"
#include <cctype>
#include <algorithm>

const std::string Name::A = "A ";
const std::string Name::a = "a ";
const std::string Name::An = "An ";
const std::string Name::an = "an ";

Name::Name(std::string name, bool proper):
name(name),proper(proper){}

std::string Name::capitalized_name() const {
    if (proper) {
        return name;
    } else if (is_vowel()) {
        return An+name;
    } else {
        return A+name;
    }
}

std::string Name::lower_case() const {
    std::string result(name);
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string Name::regular_name() const {
    if (proper) {
        return name;
    } else if (is_vowel()) {
        return an+name;
    } else {
        return a+name;
    }
}

bool Name::is_vowel() const {
    return (name[0] == 'a' || name[0] == 'e' || name[0] == 'i' ||
        name[0] == 'o' || name[0] == 'u');
}