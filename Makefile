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
SOURCES = \
	dynamics.cpp \
	imgui_impl_sdl_gl3.cpp \
	imgui/imgui.cpp \
	imgui/imgui_demo.cpp \
	imgui/imgui_draw.cpp \
	main.cpp \
	opengl.cpp \
	renderer.cpp \
	runtimecpp.cpp \
	shaderreloader.cpp \
	scene.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Headers
CPPFLAGS = \
	$(shell pkg-config --cflags $(DEPENDENCIES)) \
	-D_DEBUG \
	-I. \
	-Iimgui \
	-Iinclude

# Libraries
LDFLAGS = $(shell pkg-config --libs-only-L $(DEPENDENCIES))
LDLIBS = $(shell pkg-config --libs-only-l $(DEPENDENCIES))

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(LDLIBS) $(notdir $^) -o $@.out

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $(notdir $@)

clean:
	rm $(EXECUTABLE).out $(notdir $(OBJECTS))
