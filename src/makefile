# Compiler
CXX = g++
# Compiler flags
CXXFLAGS = -std=c++20 -Wall -Ofast -I$(HOME)/Code/adevs/include -I.
# Target executable
TARGET = server
LIBS = -ladevs -lyaml-cpp -L$(HOME)/Code/adevs

# Source files
HEADERS = model.h room.h actor.h name.h item.h types.h dice.h corpse.h trap.h maze.h coins.h limbo.h utils.h
SRCS = model.cpp room.cpp main.cpp actor.cpp name.cpp item.cpp dice.cpp corpse.cpp trap.cpp types.cpp maze.cpp coins.cpp limbo.cpp utils.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

app: objs
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

# Rule to link object files into the target executable
objs: $(OBJS) $(HEADERS)

mud: objs
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile .cpp files into .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule to remove generated files
clean:
	rm -f $(TARGET) $(OBJS)
