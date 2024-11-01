# screenshot-list/Makefile

all: screenshot-list.exe


CXX := g++

CXXFLAGS :=
CXXFLAGS += -g
CXXFLAGS += -Wall
CXXFLAGS += -Werror
CXXFLAGS += -DUNICODE
CXXFLAGS += -std=c++17

# Generate .d files.
CXXFLAGS += -MMD

LDFLAGS :=
LDFLAGS += -g
LDFLAGS += -Wall
LDFLAGS += -municode

# Disable the console window.  But note that this disables tracing
# output.
#LDFLAGS += -mwindows

# Do not require the winlibs DLLs at runtime.
LDFLAGS += -static

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

resources.o: screenshot-list.rc doc/icon.ico
	windres -o $@ $<

screenshot-list.exe: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)


.PHONY: clean
clean:
	$(RM) *.o *.d *.exe


# EOF
