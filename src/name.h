#ifndef _name_h_
#define _name_h_
#include <string>

/**
 * This class handles capitalization for
 * proper and regular names.
 */
class Name {
    public:
    /**
     * @brief Creates a name
     * @param name The proper name or noun used as a name
     * @param bool Is this a proper name or just a noun?
     */
    Name(std::string name, bool proper);
    /**
     * @brief Get a capitalized version of the the name
     * 
     * Use this to get a name for the beginning of a 
     * sentence.
     * 
     * @returns The name plus proper article
     */
    std::string capitalized_name() const;
    std::string regular_name() const;

    std::string get_name() const { return name; }
    bool is_proper() const { return proper; }

    private:

    static const std::string A;
    static const std::string a;
    static const std::string An;
    static const std::string an;

    std::string name;
    bool proper;

    bool is_vowel() const;
};

#endif