L = ../../lib

CXXFLAGS += -std=c++17 -DNATIVE -Wall -Wextra
CXXFLAGS += -I$L/hall-native -I$L/boss
CXXFLAGS += -I$L/monty -I$L/extend -I$L/arch-native -I$L/pyvm

S = $(wildcard *.cpp) \
    $(wildcard $L/hall-native/*.cpp) \
    $(wildcard $L/boss/*.cpp) \
    $(wildcard $L/monty/*.cpp) \
    $(wildcard $L/extend/*.cpp) \
    $(wildcard $L/arch-native/*.cpp) \
    $(wildcard $L/pyvm/*.cpp) \

O = $(patsubst %.cpp,%.o,$S)

all: main
	@ clear && ./main -nv

main: $O
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f $O main
