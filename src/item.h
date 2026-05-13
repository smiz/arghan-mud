#ifndef _item_h_
#define _item_h_
#include "types.h"
#include "name.h"

/**
 * @brief An item with simple state
 * 
 * Items have simple states and can be contained inside
 * of a Model. These are swords, bags, etc. They could
 * have been treated as full blown models with their own
 * dynamic state but that would have complicated handling
 * inventories of rooms and monsters. Since most items
 * will have simple states, it can simply be treated as
 * part of the Model that contains it.
 */

class Item {

    public:
    /**
     * @brief Create an item from a YAML file. 
     */
    Item(std::string file);

    std::string description() const { return m_description; }
    std::string detail() const { return m_detail; }
    Name name() const { return m_name; }
    int match_keywords(const KeyWordList& key_words) const;
    std::string filename() const { return m_filename; }

    static const std::string items_directory;

    private:

    std::shared_ptr<KeyWordList> key_words;
    Name m_name;
    std::string m_description;
    std::string m_detail;
    std::string m_filename;
};

#endif