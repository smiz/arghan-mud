#ifndef _dice_h_
#define _dice_h_
#include <random>

/**
 * Roll the bones!
 */
class Dice {

    public:

    Dice():die(0,1){}

    /**
     * @brief A set of dice
     * 
     * Creates a random number generator equivalent
     * to <number>d<die_type>+<modifier>
     * 
     * @param number Number of dice
     * @param die_type Type of die (d8, d10, d11, etc)
     * @param modifier Added to the total after rolling
     */
    Dice(int number, int die_type, int modifier = 0);
    /**
     * @brief Create a die by parsing its description
     * 
     * In the usual for, like 8d10+1
     * 
     * @param die String for die
     */
    Dice(std::string die_type);
    int operator()();

    private:

    int number;
    std::uniform_int_distribution<> die;
    int modifier;

};
#endif