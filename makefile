.PHONEY = all ec depend
INCLUDE = -Iinclude
CPPFLAGS = -std=c++20 $(INCLUDE) -fPIC
CFLAGS = -std=c11 $(INCLUDE) -fPIC
LDFLAGS = -lfmt -lglfw -lstdc++
CC = clang
CXX = clang++
C_SRCS = $(wildcard src/*.c)
CPP_SRCS = $(wildcard src/*.cpp)
SRCS = $(C_SRCS) $(CPP_SRCS)
OBJ =  $(addprefix build/, $(addsuffix .o, $(basename $(SRCS))))
DEPS = $(addprefix build/, $(addsuffix .d, $(basename $(SRCS))))

all: build/partsim 

include $(DEPS)

build/partsim: $(OBJ)
	$(CC) $^ $(LDFLAGS) -o build/partsim

build/src/%.o: src/%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

build/src/%.o: src/%.c
	$(CC) -c $(CFLAGS) $< -o $@

build/%.d: %.c*
	@mkdir -p $(@D); \
	set -e; rm -f $@; \
	$(CC) -MM $(INCLUDE) $< > $@; \
	sed -i '1s/^/build\/src\//' $@