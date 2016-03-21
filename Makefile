# Makefile for OS X.
#
# Install dependencies with Homebrew (http://brew.sh):
#
#     $ brew install pkg-config assimp sdl2
#

EXECUTABLE = fictional-doodle
LIBRARIES = assimp sdl2

# Compiler
CXX = clang++
CXXFLAGS = -g -std=c++1z -Wall -Werror

# Source
SOURCES = $(shell find . -name "*.cpp" -not -path "./include/*")

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Headers
CPPFLAGS = \
	$(shell pkg-config --cflags $(LIBRARIES)) \
	-D_DEBUG \
	-I. \
	-Iimgui \
	-Iinclude

# Libraries
LDFLAGS = $(shell pkg-config --libs-only-L $(LIBRARIES))
LDLIBS = $(shell pkg-config --libs-only-l $(LIBRARIES))

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(LDLIBS) $^ -o $@.out

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm $(EXECUTABLE).out $(OBJECTS)
