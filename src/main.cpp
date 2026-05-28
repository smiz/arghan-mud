#include <cstring>
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <filesystem>
#include <csignal>
#include <sstream>
#include "model.h"
#include "room.h"
#include "actor.h"

/**
 * @brief List of all characters in the game
 * 
 * This includes inactive characters. All of them are
 * held in memory. Characters are loaded from disk
 * when the server starts. They are saved to disk
 * as the game progresses.
 * 
 * @see Actor::save()
 */
std::map<std::string,Actor*> characters;
/// @brief List of commands from the player to inject into
/// the running simulation.
std::list<std::pair<adevs::pin_t,Event>> commands;
/// @brief Mutex to protect the simulation
std::mutex mutex;
/// @brief Condition variable for waiting on player input
std::condition_variable cv;
/// @brief The simulation itself
Simulator* sim;
/// @brief The graph that holds all of our objects in the simulation
std::shared_ptr<Graph> graph;

// Intended to support an orderly shutdown, but it doesn't
// work at the moment. The port can still be hung after 
// the server quits.
volatile bool quit_sim = false;

void run_sim() {
    Time tL = adevs_zero<Time>();
    /// enable an orderly shutdown on ctrl C
    while (!quit_sim) {
        std::unique_lock lk(mutex);
        /// Wait for the next event or the next input from a human
        Time tN = sim->nextEventTime() - tL;
        auto t_start = std::chrono::high_resolution_clock().now(); 
        std::chrono::milliseconds next(tN.real());
        cv.wait_for(lk,next);
        if (quit_sim) {
            return;
        }
        auto t_now = std::chrono::high_resolution_clock().now();
        /// Advance the clock until the we eat of the real time that has passed
        auto elapsed = (std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start)).count();
        while (sim->nextEventTime().real()-tL.real() < elapsed) {
            sim->execNextEvent();
        }
        /// Set the clock to the current time
        sim->setNextTime(Time(tL.real()+elapsed,0));
        /// Process any new characters
        tL = sim->execNextEvent();
        /// Move clock to the next instant
        sim->setNextTime(tL+adevs_epsilon<Time>());
        /// Enter the commands
        while (!commands.empty()) {
            adevs::PinValue<Event> input(commands.front().first,commands.front().second);
            commands.pop_front();
            sim->injectInput(input);
        }
        /// Execute the commands
        do {
            tL = sim->execNextEvent();
        } while(sim->nextEventTime().real() == tL.real());
    }
}

void join_game(Actor* obj) {
    Event join;
    join.type = Event::ENTER_MUD;
    join.src_id = join.dst_id = obj->id();
    join.pin = obj->pin;
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,join));
    mutex.unlock();
    cv.notify_one();
}

void leave_game(Actor* obj) {
    Event leave;
    leave.type = Event::LEAVE_MUD;
    leave.src_id = leave.dst_id = obj->id();
    leave.pin = obj->pin;
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,leave));
    mutex.unlock();
    cv.notify_one();
}

void move(Actor* obj, Direction dir) {
    Event leave;
    leave.type = Event::MOVE;
    leave.src_id = leave.dst_id = obj->id();
    leave.pin = obj->pin;
    leave.event_data.dir = dir;
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,leave));
    mutex.unlock();
    cv.notify_one();
}

void sneak(Actor* obj, bool try_to_sneak) {
    Event event(Event::SNEAK_COMMAND,obj->id());
    event.dst_id = obj->id();
    event.pin = obj->pin;
    event.stealthy = try_to_sneak;
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void kill(Actor* obj, std::list<std::string>& tokens) {
    Event event(Event::KILL_COMMAND,obj->id());
    event.pin = obj->pin;
    event.dst_id = obj->id();
    event.key_words = std::make_shared<KeyWordList>();
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void look(Actor* obj, std::list<std::string>& tokens) {
    Event event;
    event.type = Event::LOOK_COMMAND;
    event.src_id = event.dst_id = obj->id();
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    event.event_data.transfer.first_keyword_is_container = false;
    if (tokens.front() == "in") {
        tokens.pop_front();
        event.event_data.transfer.first_keyword_is_container = true;
    }
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void get(Actor* obj, std::list<std::string>& tokens) {
    Event event;
    event.type = Event::GET_COMMAND;
    event.src_id = event.dst_id = obj->id();
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    event.event_data.transfer.src_to_dst = false;
    event.event_data.transfer.first_keyword_is_container = false;
    if (tokens.front() == "from") {
        tokens.pop_front();
        event.event_data.transfer.first_keyword_is_container = true;
    }
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void drop(Actor* obj, std::list<std::string>& tokens) {
    Event event;
    event.type = Event::DROP_COMMAND;
    event.src_id = event.dst_id = obj->id();
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    event.event_data.transfer.src_to_dst = true;
    event.event_data.transfer.first_keyword_is_container = false;
    if (tokens.front() == "in") {
        tokens.pop_front();
        event.event_data.transfer.first_keyword_is_container = true;
    }
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void wield(Actor* obj, std::list<std::string>& tokens) {
    Event event;
    event.type = Event::WIELD_COMMAND;
    event.src_id = event.dst_id = obj->id();
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void hold(Actor* obj, std::list<std::string>& tokens) {
    Event event(Event::HOLD_COMMAND,obj->id());
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void wear(Actor* obj, std::list<std::string>& tokens) {
    Event event(Event::WEAR_COMMAND,obj->id());
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void stow(Actor* obj, std::list<std::string>& tokens) {
    Event event;
    event.type = Event::STOW_COMMAND;
    event.src_id = event.dst_id = obj->id();
    event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void practice(Actor* obj, std::list<std::string>& tokens) {
   Event event(Event::PRACTICE,obj->id());
   event.dst_id = obj->id();
   event.pin = obj->pin;
    event.key_words = std::make_shared<KeyWordList>();
    for (auto token: tokens) {
        event.key_words->push_back(token);
    }
    mutex.lock();
    commands.push_back(std::pair<adevs::pin_t,Event>(obj->pin,event));
    mutex.unlock();
    cv.notify_one();
}

void dice(Actor* obj, std::list<std::string>& tokens) {
    if (tokens.empty()) {
        obj->message("Roll which dice?");
        return;
    }
    try {
        std::string line;
        std::ostringstream sout(line);
        Dice die(tokens.front());
        sout << "Shake shake shake ... " << die();
        obj->message(sout.str());
    } catch(std::string msg ) {
        obj->message(msg);
    }
}

void status(Actor* obj) {
    mutex.lock();
    obj->report_stats();
    mutex.unlock();
}

void skills(Actor* obj) {
    mutex.lock();
    obj->report_skills();
    mutex.unlock();
}

void inventory(Actor *obj) {
    // Lock the simulation to avoid a change while we list the good
    mutex.lock();
    // Print the list
    obj->report_inventory();
    // Done
    mutex.unlock();
}

void toggle_short(Actor* obj) {
    mutex.lock();
    obj->toggle_short_descriptions();
    mutex.unlock();
}

void help(Actor* obj) {
    std::string line;
    std::ifstream fin("help");
    mutex.lock();
    while (std::getline(fin,line)) {
        line += "\n";
        if (write(obj->get_fd(),line.c_str(),line.size()) < 0) {
            break;
        }
    }
    mutex.unlock();
    fin.close();
}

bool parse_line_with_tokens(std::string& line, Actor* obj) {
    /// If this not a single word command, then
    /// break it into tokens
    std::istringstream sin(line);
    std::list<std::string> tokens;
    std::string token, cmd;
    sin >> cmd;
    while (sin >> token) {
        tokens.push_back(token);
    }
    if (cmd == "prac") {
        practice(obj,tokens);
        return true;
    } else if (cmd == "look") {
        look(obj,tokens);
        return true;
    } else if (cmd == "get") {
        get(obj,tokens);
        return true;
    } else if (cmd == "drop") {
        drop(obj,tokens);
        return true;
    } else if (cmd == "wield") {
        wield(obj,tokens);
        return true;
    } else if (cmd == "stow") {
        stow(obj,tokens);
        return true;
    } else if (cmd == "dice") {
        dice(obj,tokens);
        return true;
    } else if (cmd == "kill") {
        kill(obj,tokens);
        return true;
    } else if (cmd == "hold") {
        hold(obj,tokens);
        return true;
    } else if (cmd == "wear") {
        wear(obj,tokens);
        return true;
    }
    return false;
}

bool parse_line(std::string& line, Actor* obj) {
    /// Motion
    if (line == "flee") {
        move(obj,Flee);
        return true;
    }
    if (line == "e") {
        move(obj,East);
        return true;
    }
    if (line == "w") {
        move(obj,West);
        return true;
    }
    if (line == "e") {
        move(obj,East);
        return true;
    }
    if (line == "n") {
        move(obj,North);
        return true;
    }
    if (line == "s") {
        move(obj,South);
        return true;
    }
    if (line == "u") {
        move(obj,Up);
        return true;
    }
    if (line == "d") {
        move(obj,Down);
        return true;
    }
    if (line == "short") {
        toggle_short(obj);
        return true;
    }
    if (line == "inventory") {
        inventory(obj);
        return true;
    }
    if (line == "i") {
        inventory(obj);
        return true;
    }
    if (line == "stats") {
        status(obj);
        return true;
    }
    if (line == "stat") {
        status(obj);
        return true;
    }
    if (line == "skill") {
        skills(obj);
        return true;
    }
    if (line == "skills") {
        skills(obj);
        return true;
    }
    if (parse_line_with_tokens(line,obj)) {
        return true;
    }
    if (line == "help") {
        help(obj);
        return true;
    }
    if (line == "sneak") {
        sneak(obj,true);
        return true;
    }
    if (line == "nosneak") {
        sneak(obj,false);
        return true;
    }
    return false;
}

bool bad_character(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void trim(std::string& str) {
    auto iter = str.begin();
    while (iter != str.end()) { 
        if (bad_character(*iter)) {
            iter = str.erase(iter);
        } else {
            break;
        }
    }
    while (!str.empty() && bad_character(str.back())) { 
        str.pop_back();
    }
}

const std::string character_files{"characters"};
std::string character_dir = character_files+std::string("/");

std::string lower_case(std::string line) {
    std::string result(line);
    for (unsigned i = 0; i < line.size(); i++) {
        result[i] = tolower(line[i]);
    }
    return result;
}

int read_line(int fd, std::string& line) {
    const int buf_size = 128;
    char buffer[buf_size];
    int bytes = 0;
    memset(buffer,0,buf_size);
    bytes = read(fd,buffer,buf_size);
    line = buffer;
    trim(line);
    return bytes;
}

void client(int fd) {
    int bytes;
    std::string line = "What is your name? ";
    // recieving data
    bytes = write(fd,line.c_str(),line.size());
    if (bytes < 0) {
        return;
    }
repeat_name:
    bytes = read_line(fd,line);
    if (bytes <= 0) {
        line = "Goodbye!\n";
        bytes = write(fd,line.c_str(),line.size());
        return;
    }
    if (line.empty()) {
        goto repeat_name;
    }
    Actor* obj = nullptr;
    mutex.lock();
    for (auto character: characters) {
        if (character.first == lower_case(line)) {
            obj = character.second;
            line = "Welcome back " + obj->get_name().capitalized_name() + "!\n";
            bytes = write(fd,line.c_str(),line.size());
            if (bytes < 0) {
                return;
            }
            break;
        }
    }
    mutex.unlock();
    if (obj == nullptr) {
        initial_stats_t stats;
        std::string response;
        std::string name = line, passwd;
        line = "Welcome " + name +"!\n";
        bytes = write(fd,line.c_str(),line.size());
        if (bytes < 0) {
            return;
        }
        line = "Set a password: ";
        if (write(fd,line.c_str(),line.size()) < 0) {
            return;
        }
        if (read_line(fd,line) < 0) {
            return;
        }
        passwd = line;
        reroll:
        stats = Actor::initial_stats();
        {
            line.clear();
            std::ostringstream sout(line);
            sout << "str: " << stats.str << std::endl;
            sout << "dex: " << stats.dex << std::endl;
            sout << "con: " << stats.con << std::endl;
            sout << "int: " << stats.intel << std::endl;
            sout << "wis: " << stats.wis << std::endl;
            sout << "chr: " << stats.chr << std::endl;
            sout << "hp : " << stats.hp << std::endl;
            line = sout.str();
            if (write(fd,line.c_str(),line.size()) < 0) {
                return;
            }
            do {
                line = "Keep these (yes or no or quit)? ";
                if (write(fd,line.c_str(),line.size()) < 0) {
                    return;
                }
                if (read_line(fd,response) <= 0) {
                    return;
                }
            } while (response != "yes" && response != "no");
            if (response == "no") {
                goto reroll;
            }
        }
        std::string path = character_dir+name;
        mutex.lock();
        obj = new Actor(*(graph.get()),path,false,name,true,&stats);
        obj->set_password(passwd);
        obj->save();
        characters[lower_case(line)] = obj;
    } else {
        password:
        std::string msg = "Password? ";
        if (write(fd,msg.c_str(),msg.size()) <= 0) {
            close(fd);
            return;
        }
        if (read_line(fd,msg) <= 0) {
            close(fd);
            return;
        };
        if (msg != obj->get_password()) {
            goto password;
        }
        mutex.lock();
    }
    obj->set_msg_fd(fd);
    mutex.unlock();
    join_game(obj);
    while (!quit_sim) {
        bytes = read_line(fd,line);
        if (bytes <= 0) {
            break;
        }
        if (!parse_line(line,obj)) {
            if (line == "quit") {
                break;
            } else {
                line = "What?\n";
                bytes = write(fd,line.c_str(),line.size());
                if (bytes < 0) {
                    break;
                }
            }
        }
    }
    if (quit_sim) {
        close(fd);
        return;
    }
    leave_game(obj);
    // closing the socket.
    if (fd == 0) {
        mutex.lock();
        exit(0);
    }
    close(fd);
}

void create_sim() {
    graph = std::make_shared<Graph>();
    const std::filesystem::path room_path{"rooms"};
    const std::filesystem::path char_path{character_dir};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(room_path)) {
        if (std::filesystem::is_regular_file(entry)) {
            std::cout << "Loading room " << entry.path() << std::endl;
            // Room adds itself to the graph
            new Room(*(graph.get()),entry.path());
        }
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(char_path)) {
        if (std::filesystem::is_regular_file(entry)) {
            std::cout << "Loading character " << entry.path() << std::endl;
            // Room adds itself to the graph
            Actor* obj = new Actor(*(graph.get()),entry.path(),true,"",true);
            characters[obj->get_name().lower_case()] = obj;
        }
    }
    sim = new Simulator(graph);
    std::thread sim_thread(run_sim);
    sim_thread.detach();
}

int serverSocket;

void ctrlCHandler(int dummy) {
    quit_sim = true;
    close(serverSocket);
}

int main(int argc, char** argv) {
    // Create the simulator
    create_sim();
    if (argc < 2) {
        std::thread thrd(client,0);
        thrd.join();
        return 0;
    }
    std::signal(SIGINT,ctrlCHandler);
    short port = atoi(argv[1]);
    // Ignore SIGNAL when a telnet client leaves
    std::signal(SIGHUP,SIG_IGN);
    std::signal(SIGPIPE,SIG_IGN);
    // creating socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));
    while (!quit_sim) {
        // listening to the assigned socket
        listen(serverSocket, 5);
        // accepting connection request
        int clientSocket
            = accept(serverSocket, nullptr, nullptr);
        std::thread thrd(client,clientSocket);
        thrd.detach();
    }
    /// Make sure the simulator isn't running so that we
    /// don't quit while writing any save files
    cv.notify_one();
    mutex.lock();
    /// Call it a day
    return 0;
}