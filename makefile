.PHONEY := all clean
INCLUDE := -Iinclude -I. -Iexternal/tuplet/include
FLAGS := -fPIC -fexceptions -g -O3 \
-DVK_USE_PLATFORM_WAYLAND_KHR -DVULKAN_HPP_NO_CONSTRUCTORS \
`sdl2-config --cflags` -flto=thin
DEP_FLAGS =  -MMD -MF $(addsuffix .d,$(basename $@))
CPPFLAGS := -std=c++20 $(INCLUDE) $(FLAGS)
LDFLAGS := -lfmt -lvulkan `sdl2-config --libs` -flto=thin
CXX := clang++
SRCS := $(shell find src/ -type f -name '*.cpp')
OBJ :=  $(addprefix build/, $(addsuffix .o, $(basename $(SRCS))))
BUILD_DIR  := build
# yes this is absolutely unportable no I cant find a better way around it
DEPS :=$(shell find $(BUILD_DIR) -type f -name "*.d"  -print 2> /dev/null)

all: build/partsim

clean:
	rm -rdf build

build/partsim: $(OBJ)
	$(CXX) $^ $(LDFLAGS) -o $@

build/%.o: %.cpp build/vert.hpp build/frag.hpp
	@mkdir -p $(@D)
	$(CXX) -c $(CPPFLAGS) $(DEP_FLAGS) $< -o $@

build/shaders/vert.spv: shaders/shader.vert
	@mkdir -p $(@D)
	glslangValidator $^ --quiet -V100 -gVS -o $@

build/vert.hpp: build/shaders/vert.spv
	@xxd -i $^ $@
	@sed -i '1s/^/#pragma once \ninline constexpr /' $@

build/shaders/frag.spv: shaders/shader.frag
	@mkdir -p $(@D)
	glslangValidator $^ --quiet -V100 -gVS -o $@

build/frag.hpp: build/shaders/frag.spv
	@xxd -i $^ $@
	@sed -i '1s/^/#pragma once \ninline constexpr /' $@

include external/imgui.mk
include $(DEPS)