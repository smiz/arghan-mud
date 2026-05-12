#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <filesystem>
#include <csignal>
#include "model.h"
#include "room.h"
#include "actor.h"

std::map<std::string,Actor*> characters;
std::list<std::pair<adevs::pin_t,Event>> commands;
std::mutex mutex;
std::condition_variable cv;
Simulator* sim;
std::shared_ptr<Graph> graph;

void run_sim() {
    Time tL = adevs_zero<Time>();
    for (;;) {
        std::unique_lock lk(mutex);
        /// Wait for the next event or the next input from a human
        Time tN = sim->nextEventTime() - tL, tTmp = tL;
        auto t_start = std::chrono::high_resolution_clock().now(); 
        std::chrono::milliseconds next(tN.real());
        cv.wait_for(lk,next);
        auto t_now = std::chrono::high_resolution_clock().now();
        /// Advance the clock until the we eat of the real time that has passed
        auto elapsed = (std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start)).count();
        while (sim->nextEventTime().real()-tL.real() < elapsed) {
            tTmp = sim->execNextEvent();
        }
        /// Set the clock to the current time
        sim->setNextTime(Time(tL.real()+elapsed,0));
        tL = tTmp;
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

bool parse_line(std::string& line, Actor* obj) {
    /// Motion
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

int read_line(int fd, std::string& line) {
    const int buf_size = 1024;
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
    write(fd,line.c_str(),line.size());
repeat_name:
    bytes = read_line(fd,line);
    if (bytes <= 0) {
        line = "Goodbye!\n";
        write(fd,line.c_str(),line.size());
        return;
    }
    if (line.empty()) {
        goto repeat_name;
    }
    Actor* obj = nullptr;
    mutex.lock();
    for (auto character: characters) {
        if (character.first == line) {
            obj = character.second;
            line = "Welcome back " + character.first + "!\n";
            write(fd,line.c_str(),line.size());
            break;
        }
    }
    if (obj == nullptr) {
        graph->set_provisional(false);
        std::string path = character_dir+line;
        obj = new Actor(*(graph.get()),path,false,line);
        graph->set_provisional(true);
        characters[line] = obj;
        line = "Welcome " + line + "!\n";
        write(fd,line.c_str(),line.size());
    }
    mutex.unlock();
    obj->set_msg_fd(fd);
    join_game(obj);
    for (;;) {
        bytes = read_line(fd,line);
        if (bytes <= 0) {
            break;
        }
        if (!parse_line(line,obj)) {
            if (line == "quit") {
                break;
            } else {
                line = "What?\n";
                write(fd,line.c_str(),line.size());
            }
        }
    }
    leave_game(obj);
    // closing the socket.
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
            Actor* obj = new Actor(*(graph.get()),entry.path(),true);
            characters[obj->get_name()] = obj;
        }
    }
    sim = new Simulator(graph);
    std::thread sim_thrd(run_sim);
    sim_thrd.detach();
}

int main(int argc, char** argv) {
    // Create the simulator
    create_sim();
    if (argc < 2) {
        std::thread thrd(client,0);
        thrd.join();
        return 0;
    }
    short port = atoi(argv[1]);
    // Ignore SIGNAL when a telnet client leaves
    std::signal(SIGHUP,SIG_IGN);
    std::signal(SIGPIPE,SIG_IGN);
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));
    for (;;) {
        // listening to the assigned socket
        listen(serverSocket, 5);
        // accepting connection request
        int clientSocket
            = accept(serverSocket, nullptr, nullptr);
        std::thread thrd(client,clientSocket);
        thrd.detach();
    }
    return 0;
}