# Compiler
CXX = g++
# Compiler flags
CXXFLAGS = -Wall -O -g -I$(HOME)/Code/adevs/include -I.
# Target executable
TARGET = server
LIBS = -ladevs -lyaml-cpp -L$(HOME)/Code/adevs

# Source files
HEADERS = model.h room.h actor.h name.h item.h types.h dice.h
SRCS = model.cpp room.cpp main.cpp actor.cpp name.cpp item.cpp dice.cpp

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
