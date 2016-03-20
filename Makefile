# Makefile for OS X.
#
# Install dependencies with Homebrew (http://brew.sh):
#
#     $ brew install pkg-config assimp sdl2
#

EXECUTABLE = fictional-doodle
DEPENDENCIES = assimp sdl2

# Compiler
CXX = clang++
CXXFLAGS = -g -std=c++14 -Wall -Werror

# Source
SOURCES = main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Headers
CPPFLAGS = -D_DEBUG -Iinclude

# Libraries
LDFLAGS = $(shell pkg-config --libs-only-L $(DEPENDENCIES))
LDLIBS = $(shell pkg-config --libs-only-l $(DEPENDENCIES))

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(LDLIBS) $^ -o $@.out

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

clean:
	rm $(EXECUTABLE).out $(OBJECTS)
