cmake_minimum_required(VERSION 3.16.0)

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

add_executable(psim ${PROJECT_SRC}/main.cpp ${PROJECT_SRC}/glad.c
                    ${PROJECT_SRC}/assets.c)
target_include_directories(psim PRIVATE include)
target_link_libraries(psim OpenGL glfw fmt)

add_dependencies(psim shaders)
target_compile_features(psim PRIVATE cxx_std_20)
target_compile_features(psim PRIVATE c_std_23)
add_custom_command(
  TARGET psim
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:psim>/shaders/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders"
          "$<TARGET_FILE_DIR:psim>/shaders")
