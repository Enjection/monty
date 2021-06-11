# this makefile is for use by tdd.py

LIBS = ../lib
DIRS = doctest hall-native boss monty extend arch-native pyvm

# everything below is essentially automatic

VPATH = $(patsubst %,$(LIBS)/%,$(DIRS))
SRCS = $(wildcard *.cpp) $(foreach D,$(DIRS),$(wildcard $(LIBS)/$D/*.cpp))

tdd: ; @ while ./tdd.py . $(LIBS); do :; done

all: cls run

cls:  ; @ clear && date
run:  ; @ cd .. && inv x-doctest

# used by tdd.py
dirs: ; @ echo $(patsubst %,$(LIBS)/%,$(DIRS))
srcs: ; @ echo $(SRCS)
clean:
