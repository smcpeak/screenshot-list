# screenshot-list/Makefile

all:


CXX := g++

CXXFLAGS :=
CXXFLAGS += -g
CXXFLAGS += -Wall
CXXFLAGS += -Werror
CXXFLAGS += -DUNICODE
CXXFLAGS += -std=c++17

# Generate .d files.
CXXFLAGS += -MMD

# Linker flags.
LDFLAGS :=
LDFLAGS += -g
LDFLAGS += -Wall
LDFLAGS += -municode

# Do not require the winlibs DLLs at runtime.
LDFLAGS += -static

# Linker flags when creating a GUI.
GUI_LDFLAGS := $(LDFLAGS)

# Disable the console window.  But note that this disables tracing
# output.
GUI_LDFLAGS += -mwindows

LIBS :=

# SelectObject, etc.
LIBS += -lgdi32

OBJS :=
OBJS += base-window.o
OBJS += dcx.o
OBJS += screenshot-list.o
OBJS += resources.o
OBJS += screenshot.o
OBJS += trace.o
OBJS += winapi-util.o


-include $(wildcard *.d)

%.o: %.cc
	$(CXX) -c -o $@ $(CXXFLAGS) $<

all: winapi-util-test.exe
winapi-util-test.exe: trace.o winapi-util.o winapi-util-test.o
	$(CXX) -o $@ $(LDFLAGS) $^ $(LIBS)

resources.o: screenshot-list.rc doc/icon.ico
	windres -o $@ $<

all: screenshot-list.exe
screenshot-list.exe: $(OBJS)
	$(CXX) -o $@ $(GUI_LDFLAGS) $(OBJS) $(LIBS)


.PHONY: clean
clean:
	$(RM) *.o *.d *.exe


# EOF
