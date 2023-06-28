#pragma once

#include "glfwsetup.hpp"

struct Context;
struct Renderer;

Context setupVk(GLFWwin &&);
Renderer setupRenderer(Context &c);