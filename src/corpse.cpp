#include "corpse.h"

Corpse::Corpse(Name name):
Item() {
    m_name = Name("corpse",false);
    m_description = "You see the earthly remains of "+name.regular_name()+".";
    m_detail = "The soul has parted and the clay returns to the earth. This corpse smells bad.";
    key_words->push_back("corpse");
    key_words->push_back("remains");
    key_words->push_back(name.get_name());
    if (name.get_name() != name.lower_case()) {
        key_words->push_back(name.lower_case());
    }
    m_contents = std::make_shared<std::list<std::shared_ptr<Item>>>();
}