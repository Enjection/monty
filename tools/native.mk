# make-based TDD runner - everything below is essentially automatic

CXXFLAGS = $(OPTS)
CXXFLAGS += -DNATIVE -std=c++17
CXXFLAGS += $(patsubst %,-I$(LIBS)/%,$(DIRS))
LDFLAGS += -pthread
VPATH = $(patsubst %,$(LIBS)/%,$(DIRS))
SRCS = $(wildcard *.cpp) $(foreach D,$(DIRS),$(wildcard $(LIBS)/$D/*.cpp))
OBJS = $(patsubst %.cpp,%.o,$(notdir $(SRCS)))

ifneq ($(wildcard /dev/gpiomem),)  
CXXFLAGS += -DRASPI
endif

# not really an OS issue: clang on MacOS doesn't like this option
ifeq ($(shell uname -s),Linux)
OPTS += -Wno-cast-function-type
endif

tdd:
	@ fswatch -h >/dev/null # make sure it's installed
	@ while $(ROOT)/tools/tdd.py . $(LIBS); do :; done

all: cls main run

cls:
	@ clear && date
main: $(OBJS)
	@ $(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
run:
	@ ./main -nv -ne

%.o: %.cpp
	@ echo $(CXX) $<
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

# used by tdd.py
dirs: ; @ echo $(patsubst %,$(LIBS)/%,$(DIRS))
srcs: ; @ echo $(SRCS)

# keep main.o, as it is slow to compile and never changes
clean: 	   ; bash -c 'GLOBIGNORE=main.o; rm -f *.o main' # only works in bash
distclean: ; rm -f *.o main
