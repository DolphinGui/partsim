cmake_minimum_required(VERSION 3.16.1)

project(partsim CXX C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_SRC ${CMAKE_SOURCE_DIR}/src)
include(CTest)
include(cmake/cpm.cmake)
include(cmake/shader.cmake)

find_package(OpenGL)
cpmaddpackage(
  NAME
  GLFW
  GITHUB_REPOSITORY
  glfw/glfw
  GIT_TAG
  3.3.8
  OPTIONS
  "GLFW_BUILD_TESTS OFF"
  "GLFW_BUILD_EXAMPLES OFF"
  "GLFW_BULID_DOCS OFF"
  "GLFW_USE_WAYLAND ON")
cpmaddpackage(NAME fmt GIT_TAG 6.1.2 GITHUB_REPOSITORY fmtlib/fmt)

add_executable(partsim ${PROJECT_SRC}/main.cpp ${PROJECT_SRC}/glad.c
                    ${PROJECT_SRC}/assets.c)
target_include_directories(partsim PRIVATE include)
target_link_libraries(partsim vulkan glfw fmt)

target_compile_features(partsim PRIVATE cxx_std_20)
target_compile_features(partsim PRIVATE c_std_23)