LIBS = ../../lib
DIRS = hall-native boss monty extend arch-native pyvm

CXXFLAGS += -std=c++17 -DNATIVE -Wall -Wextra
CXXFLAGS += $(patsubst %,-I$(LIBS)/%,$(DIRS))

VPATH = $(patsubst %,$(LIBS)/%,$(DIRS))

SRCS = \
	$(wildcard *.cpp) \
	$(wildcard $(LIBS)/hall-native/*.cpp) \
	$(wildcard $(LIBS)/boss/*.cpp) \
	$(wildcard $(LIBS)/monty/*.cpp) \
	$(wildcard $(LIBS)/extend/*.cpp) \
	$(wildcard $(LIBS)/arch-native/*.cpp) \
	$(wildcard $(LIBS)/pyvm/*.cpp) \

OBJS = $(patsubst %.cpp,%.o,$(notdir $(SRCS)))

all: cls main
	@ ./main -nv || true

cls:
	@ clear && date

main: $(OBJS)
	@ $(CXX) $(LDFLAGS) $(^F) $(LDLIBS) -o $@

%.o: %.cpp
	@ echo $(CXX) $<
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

clean:
	rm -f *.o main
