#ifndef _dice_h_
#define _dice_h_
#include <random>

/**
 * Roll the bones!
 */
class Dice {

    public:

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
    int operator()();

    private:

    const int number;
    std::uniform_int_distribution<> die;
    const int modifier;

};
#endif