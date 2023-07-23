.PHONEY := all clean
INCLUDE := -Iinclude -I. -Iexternal/tuplet/include -Iexternal/imgui -Iexternal
FLAGS := -fPIC -fexceptions -g -O3 \
-DVK_USE_PLATFORM_WAYLAND_KHR -DVULKAN_HPP_NO_CONSTRUCTORS \
`sdl2-config --cflags` -flto=thin
DEP_FLAGS =  -MMD -MF $(addsuffix .d,$(basename $@))
CPPFLAGS := -std=c++20 $(INCLUDE) $(FLAGS)
LDFLAGS := -lfmt -lvulkan `sdl2-config --libs` -flto=thin
CXX := clang++
BACKENDS := external/imgui/backends
SRCS = $(shell find src/ -type f -name '*.cpp') \
$(wildcard external/imgui/*.cpp) $(BACKENDS)/imgui_impl_sdl2.cpp \
$(BACKENDS)/imgui_impl_sdl2.cpp $(BACKENDS)/imgui_impl_vulkan.cpp
OBJ :=  $(addprefix build/, $(addsuffix .o, $(basename $(SRCS))))
DEPS :=$(addsuffix .d, $(basename $(SRCS)))

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

-include $(DEPS)