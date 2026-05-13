#ifndef _actor_h_
#define _actor_h_
#include "model.h"
#include "name.h"
#include "item.h"

/**
 * Models a creature. These should be 
 * instantiated after the rooms so that room
 * events always happen before actor events.
 */
class Actor: public Model {

    public:

    /**
     * @brief Create an actor from a YAML file.
     * 
     * If fd = -1 this actor will not produce text.
     * Otherwise, messages are written to the file
     * descriptor.
     * 
     * @param graph The world that the actor belongs in
     * @param filename Where the actor data is stored
     * @param load If true, load from the file. Write to it is false.
     * @param name Name of the new actor
     * @param pc Is this a player character?
     */
    Actor(Graph& graph,
        std::string filename,
        bool load,
        std::string name = "",
        bool pc = false);

    Name get_name() const { return name; }
    void set_msg_fd(int fd) { this->fd = fd; }
    int match_keywords(const KeyWordList& key_words) const;
    void report_inventory();

    protected:

    std::string detail;
    std::string description;
    Name name;
    std::string file;
    KeyWordList key_words;

    /// @brief Write a message to the console if fd is valid
    /// @param msg The message to write 
    void message(std::string msg);

    void look_command_event(const Event& event);
    void get_command_event(const Event& event);
    void drop_command_event(const Event& event);
    void join_prox_group_event(const Event& event);
    void leave_prox_group_event(const Event& event);
    void enter_mud_event(const Event& event);
    void leave_mud_event(const Event& event);
    void see_event(const Event& event);
    void quit_mud_event(const Event& event);
    void move_event(const Event& event);
    void look_event(const Event& event);
    void transfer_item_event(const Event& event);
    void save_model_event(const Event& event);

    private:

    void change_prox_groups(int new_group);
    void save();
    void init();
    void sched_save();

    int fd;
    /// @brief Node at which the actor left the mud
    int exit_node_id;
    /// @brief Is that actor pc that should be saved?
    const bool pc;
};

#endif