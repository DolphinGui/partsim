.PHONEY = all clean
INCLUDE = -Iinclude -I.
FLAGS = -fPIC -fexceptions -g -O3 \
-DVK_USE_PLATFORM_WAYLAND_KHR -DVULKAN_HPP_NO_CONSTRUCTORS \
`sdl2-config --cflags` -fno-omit-frame-pointer
CPPFLAGS = -std=c++20 $(INCLUDE) $(FLAGS)
CFLAGS = -std=c11 $(INCLUDE) $(FLAGS)
LDFLAGS = -lfmt -lvulkan `sdl2-config --libs`
CC = clang
CXX = clang++
C_SRCS =  $(shell find src/ -type f -name '*.c')
CPP_SRCS = $(shell find src/ -type f -name '*.cpp')
SRCS = $(C_SRCS) $(CPP_SRCS)
OBJ =  $(addprefix build/, $(addsuffix .o, $(basename $(SRCS))))
DEPS = $(addprefix build/, $(addsuffix .d, $(basename $(SRCS))))

all: build/partsim

clean:
	rm -rdf build

include $(DEPS)

build/vulkan.hpp.pch:
	clang++ $(CPPFLAGS) -x c++-header /usr/include/vulkan/vulkan.hpp -o $@

build/vulkan_raii.hpp.pch:
	clang++ $(CPPFLAGS) -x c++-header /usr/include/vulkan/vulkan_raii.hpp -o $@

build/partsim: $(OBJ)
	$(CXX) $^ $(LDFLAGS) -o build/partsim

# pch header must be included manually otherwise pch needs itself
build/src/%.o: src/%.cpp build/vulkan.hpp.pch build/vulkan_raii.hpp.pch
	$(CXX) -c $(CPPFLAGS) -include-pch build/vulkan.hpp.pch -include-pch build/vulkan_raii.hpp.pch $< -o $@

build/src/%.o: src/%.c
	$(CC) -c $(CFLAGS) $< -o $@

build/%.d: %.c*
	@mkdir -p $(@D)
	@set -e rm -f $@
	@$(CC) -MM -MG $(INCLUDE) $< -o $@
	@sed -i '1s/^/$(subst /,\/,$@) $(subst /,\/, $(@D))\//' $@

build/shaders/vert.spv: shaders/shader.vert
	@mkdir -p $(@D)
	glslc $^ -o $@

build/vert.hpp: build/shaders/vert.spv
	xxd -i $^ $@
	@sed -i '1s/^/#pragma once \ninline constexpr /' $@

build/shaders/frag.spv: shaders/shader.frag
	@mkdir -p $(@D)
	glslc $^ -o $@

build/frag.hpp: build/shaders/frag.spv
	xxd -i $^ $@
	@sed -i '1s/^/#pragma once \ninline constexpr /' $@