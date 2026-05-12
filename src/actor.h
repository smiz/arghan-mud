#ifndef _actor_h_
#define _actor_h_
#include "model.h"

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
     */
    Actor(Graph& graph, std::string filename, bool load, std::string name = "", int exit_node = START_GROUP);

    std::string get_name() const { return name; }
    void set_msg_fd(int fd) { this->fd = fd; }

    protected:

    std::string description;
    std::string name;
    std::string file;

    /// @brief Write a message to the console if fd is valid
    /// @param msg The message to write 
    void message(std::string msg);

    void join_prox_group_event(const Event& event);
    void leave_prox_group_event(const Event& event);
    void enter_mud_event(const Event& event);
    void leave_mud_event(const Event& event);
    void see_event(const Event& event);
    void quit_mud_event(const Event& event);
    void move_event(const Event& event);

    private:

    void change_prox_groups(int new_group);
    void save();

    int fd;
    /// @brief Node at which the actor left the mud
    int exit_node_id;
};

#endif