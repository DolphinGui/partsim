.PHONEY = all clean ec
INCLUDE = -Iinclude -I.
FLAGS = -fPIC -fexceptions -DGLFW_EXPOSE_NATIVE_WAYLAND -DVK_USE_PLATFORM_WAYLAND_KHR -DVULKAN_HPP_NO_CONSTRUCTORS -g -O3
CPPFLAGS = -std=c++20 $(INCLUDE) $(FLAGS) 
CFLAGS = -std=c11 $(INCLUDE) $(FLAGS)
LDFLAGS = -lfmt -lglfw  -lvulkan 
CC = clang
CXX = clang++
C_SRCS =  $(shell find src/ -type f -name '*.c')
CPP_SRCS = $(shell find src/ -type f -name '*.cpp')
SRCS = $(C_SRCS) $(CPP_SRCS)
OBJ =  $(addprefix build/, $(addsuffix .o, $(basename $(SRCS))))
DEPS = $(addprefix build/, $(addsuffix .d, $(basename $(SRCS))))

all: build/vert.hpp build/partsim

clean:
	rm -rdf build

include $(DEPS)

ec:
	echo $(RECURSE_CPP)

build/partsim: $(OBJ)
	$(CXX) $^ $(LDFLAGS) -o build/partsim

build/src/%.o: src/%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

build/src/%.o: src/%.c
	$(CC) -c $(CFLAGS) $< -o $@

build/%.d: %.c*
	@mkdir -p $(@D); \
	set -e; rm -f $@; \
	$(CC) -MM -MG $(INCLUDE) $< > $@; \
	sed -i '1s/^/$(subst /,\/,$@) $(subst /,\/, $(@D))\//' $@

build/shaders/vert.spv: shaders/shader.vert
	@mkdir -p $(@D); \
	glslc $^ -o $@

build/vert.hpp: build/shaders/vert.spv
	@xxd -i $^ $@; \
	sed -i '1s/^/#pragma once \ninline constexpr /' $@