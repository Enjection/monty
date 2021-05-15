# this makefile is for use by tdd.py

LIBS = ../../lib
DIRS = hall-native boss monty extend arch-native pyvm
CXXFLAGS = -std=c++17 -DNATIVE -Wall -Wextra

# everything below is essentially automatic

CXXFLAGS += -DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
CXXFLAGS += $(patsubst %,-I$(LIBS)/%,$(DIRS))
LDFLAGS += -pthread
VPATH = $(patsubst %,$(LIBS)/%,$(DIRS))
SRCS = $(wildcard *.cpp) $(foreach D,$(DIRS),$(wildcard $(LIBS)/$D/*.cpp))
OBJS = $(patsubst %.cpp,%.o,$(notdir $(SRCS)))

tdd: ; @ while ./tdd.py . $(LIBS); do :; done

all: cls main
	@ ./main -nv -ne

cls: ; @ clear && date

main: $(OBJS)
	@ $(CXX) $(LDFLAGS) $(^F) $(LDLIBS) -o $@

%.o: %.cpp
	@ echo $(CXX) $<
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

dirs: ; @ echo $(patsubst %,$(LIBS)/%,$(DIRS))
srcs: ; @ echo $(SRCS)

clean: ; rm -f *.o main
