#ifndef _item_h_
#define _item_h_
#include "types.h"
#include "name.h"
#include "dice.h"

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
    Item();
    virtual ~Item(){}

    std::string description() const { return m_description; }
    std::string detail() const { return m_detail; }
    Name name() const { return m_name; }
    int match_keywords(const KeyWordList& key_words) const;
    void set_filename(std::string filename) { m_filename = filename; }
    std::string filename() const { return m_filename; }

    void save();

    static const std::string items_directory;

    struct weapon_info_t {
        Dice dmg_die;
        int speed;
        weapon_info_t():speed(0){}
    };

    weapon_info_t& weapon_info() {
        return m_weapon_info;
    }

    /// @brief Which attribute modifies the use of this item
    MonsterAttributes modifier() const { return m_modifier; }
    /// @brief Which skill modifies the use of this item
    Skill skill() const { return m_skill; };
    /// @brief Where can this item be worn?
    WearableSlots wearable() const { return m_wearable; }
    /// @brief What is the ac bonus when the item is held?
    int ac_bonus_when_held() const { return m_held_ac_bonus; }
    /// @brief What is the ac bonus when the item is worn?
    int ac_bonus_when_worn() const { return m_worn_ac_bonus; }
    /// @brief Can this item hold other items?
    bool container() const { return m_contents != nullptr; }
    /// @brief Get the contents. Only valid if this is a container.
    const std::list<std::shared_ptr<Item>>& contents() {
        return *(m_contents.get());
    }
    void add_item(std::shared_ptr<Item> item) {
        m_contents->push_back(item);
    }
    /// @brief Find best match and remove it
    /// @param key_words The keywords to search on
    /// @return An item or nullptr if no matches
    std::shared_ptr<Item> remove_item(const KeyWordList& key_words);

    /**
     * Delete all items from a container. They are gone forever.
     * This is used to load player data where all inventory is
     * stored flat.
     */
    void clear() {
        m_contents->clear();
    }

    int claim_xp() {
        int tmp = xp;
        xp = 0;
        return tmp;
    }

    int get_cost() const;
    bool is_locked() const { return locked; }
    int get_key_code() const { return m_key; }
    int get_lock_code() const { return m_lock; }
    bool is_heavy() const { return heavy; }
    void toggle_lock() { locked = !locked; }
    const std::string& get_message() const { return m_message; }
    int get_message_complexity() const { return m_message_complexity; }

    protected:

    int xp;
    int m_held_ac_bonus;
    int m_worn_ac_bonus;
    int m_cost;
    int m_lock, m_key;
    bool locked;
    bool heavy;
    std::string m_message;
    int m_message_complexity;
    WearableSlots m_wearable;
    MonsterAttributes m_modifier;
    Skill m_skill;
    std::shared_ptr<KeyWordList> key_words;
    Name m_name;
    std::string m_description;
    std::string m_detail;
    std::string m_filename;
    weapon_info_t m_weapon_info;
    std::shared_ptr<std::list<std::shared_ptr<Item>>> m_contents;
};

#endif