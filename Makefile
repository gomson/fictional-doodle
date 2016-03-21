# Makefile for OS X
#
# Install dependencies via Homebrew (http://brew.sh):
#
#     $ brew install pkg-config assimp sdl2
#

EXECUTABLE = fictional-doodle.out
LIBRARIES = assimp sdl2

CXX = clang++
CXXFLAGS = -g -std=c++1z -Wall -Werror

# Create a hidden build directory for object and dependency files.
BUILDDIR = .build
$(shell mkdir -p $(BUILDDIR) >/dev/null)

# Compile all source files excluding those in "include".
SOURCES = $(shell find . -name "*.cpp" -not -path "./include/*" | sed "s/\.\///")
OBJECTS = $(SOURCES:%.cpp=$(BUILDDIR)/%.o)
DEPENDENCIES = $(SOURCES:%.cpp=$(BUILDDIR)/%.d)

# Compile flags to generate dependency files.
DEPFLAGS = -MMD -MP -MF $(BUILDDIR)/$*.Td
CPPFLAGS = -D_DEBUG -Iinclude $(shell pkg-config --cflags $(LIBRARIES))
LDFLAGS = $(shell pkg-config --libs-only-L $(LIBRARIES))
LDLIBS = $(shell pkg-config --libs-only-l $(LIBRARIES))

# Build executable.
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Build object and dependency file, mirroring the source directory hierarchy
# within the build directory.
$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@
	@mv -f $(BUILDDIR)/$*.Td $(BUILDDIR)/$*.d

.PHONY: clean
clean:
	rm -rf $(EXECUTABLE) $(BUILDDIR)

# Track header file changes.
-include $(DEPENDENCIES)
