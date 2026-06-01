#ifndef _coins_h_
#define _coins_h_
#include "item.h"

class Coins: public Item {

    public:

    Coins(int count);
    void increment(int count);
    void decrement(int count);

    private:

    void set_description();
};

#endif
