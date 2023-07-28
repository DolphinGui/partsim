.PHONEY := all clean
INCLUDE := -Iinclude -I. -Iexternal/tuplet/include -Iexternal/imgui -Iexternal
FLAGS := -fPIC -fexceptions -g -O3 \
-DVK_USE_PLATFORM_WAYLAND_KHR -DVULKAN_HPP_NO_CONSTRUCTORS -DVULKAN_HPP_NO_STRUCT_SETTERS\
`sdl2-config --cflags`
DEP_FLAGS =  -MMD -MF $(addsuffix .d,$(basename $@))
CPPFLAGS := -std=c++20 $(INCLUDE) $(FLAGS)
LDFLAGS := -lfmt -lvulkan `sdl2-config --libs`
CXX := clang++
BACKENDS := external/imgui/backends
SRCS = $(shell find src/ -type f -name '*.cpp') \
$(wildcard external/imgui/*.cpp) $(BACKENDS)/imgui_impl_sdl2.cpp \
$(BACKENDS)/imgui_impl_sdl2.cpp $(BACKENDS)/imgui_impl_vulkan.cpp
OBJ :=  $(addprefix build/, $(addsuffix .o, $(basename $(SRCS))))
DEPS := $(addprefix build/,$(addsuffix .d, $(basename $(SRCS))))

all: build/partsim

clean:
	rm -rdf build

build/partsim: $(OBJ)
	$(CXX) $^ $(LDFLAGS) -o $@

build/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CPPFLAGS) $(DEP_FLAGS) $< -o $@

build/src/setup/vksetup.o: build/vert.hpp build/frag.hpp build/comp.hpp

build/%.hpp: shaders/*.%
	@mkdir -p $(@D)
	glslangValidator $^ --quiet -V100 --vn $(basename $(notdir $@)) -o $@

-include $(DEPS)