# this makefile is for use by tdd.py

LIBS = ../../lib
DIRS = doctest hall-native boss
CXXFLAGS = -std=c++17 -Wall -Wextra

# everything below is essentially automatic

CXXFLAGS += -DNATIVE -DDOCTEST -DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
CXXFLAGS += $(patsubst %,-I$(LIBS)/%,$(DIRS))
LDFLAGS += -pthread
VPATH = $(patsubst %,$(LIBS)/%,$(DIRS))
SRCS = $(wildcard *.cpp) $(foreach D,$(DIRS),$(wildcard $(LIBS)/$D/*.cpp))
OBJS = $(patsubst %.cpp,%.o,$(notdir $(SRCS)))

ifneq ($(wildcard /dev/gpiomem),)  
CXXFLAGS += -DRASPI
endif

tdd: ; @ while ../../src/tdd.py . $(LIBS); do :; done

all: cls main run

cls: 		  ; @ clear && date
main: $(OBJS) ; @ $(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
run: 		  ; @ ./main -nv -ne

%.o: %.cpp
	@ echo $(CXX) $<
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

dirs: ; @ echo $(patsubst %,$(LIBS)/%,$(DIRS))
srcs: ; @ echo $(SRCS)

# keep main.o, as it is slow to compile and never changes
clean: 	   ; bash -c 'GLOBIGNORE=main.o; rm -f *.o main' # only works in bash
distclean: ; rm -f *.o main
