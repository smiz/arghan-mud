#ifndef _dice_h_
#define _dice_h_
#include <random>

/**
 * Roll the bones!
 */
class Dice {

    public:

    /// @brief Default always rolls a zero
    Dice():die(0,1){}

    /**
     * @brief A set of dice
     * 
     * Creates a random number generator equivalent
     * to <number>d<die_type>+<modifier>.
     * 
     * @param number Number of dice
     * @param die_type Type of die (d8, d10, d11, etc)
     * @param modifier Added to the total after rolling
     */
    Dice(int number, int die_type, int modifier = 0);
    /**
     * @brief Create a die by parsing its description
     * 
     * In the usual form, like 8d10+1. Throws a string 
     * describing the error (badly) if the die_type
     * can't be parsed.
     * 
     * @param die_type String for die
     */
    Dice(std::string die_type);
    /// @brief Roll the die!
    /// @return The result of the roll
    int operator()();

    private:

    int number;
    std::uniform_int_distribution<> die;
    int modifier;

};
#endif