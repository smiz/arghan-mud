#include "coins.h"
#include <sstream>

Coins::Coins(int count):
Item() {
    m_cost = count;
    m_detail = "These shiny coins are useful for trade.";
    set_description();
    key_words->push_back("coins");
    key_words->push_back("coin");
    m_contents = nullptr;
}

void Coins::increment(int count) {
    m_cost += count;
    set_description();
}

void Coins::decrement(int count) {
    m_cost -= count;
    set_description();
}

void Coins::set_description() {
	std::string line;
	std::ostringstream sout(line);
	sout << m_cost << " coins";
    m_description = "You see " + sout.str()+".";
    m_name = Name("pile of " + sout.str(),false);
}