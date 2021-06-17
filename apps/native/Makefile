# make-based TDD runner - this makefile uses tdd.py and fswatch

ROOT = ../..
LIBS = $(ROOT)/common
DIRS = monty
OPTS = -Wall -Wextra

# not really an OS issue: clang on MacOS doesn't like this option
ifeq ($(shell uname -s),Linux)
OPTS += -Wno-cast-function-type
endif

include $(ROOT)/scripts/native.mk
