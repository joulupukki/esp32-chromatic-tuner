# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS "extra_components/q/q_lib" "extra_components/q-infra")

project(esp32-chromatic-tuner)

link_libraries(PUBLIC libq)
link_libraries(PUBLIC infra)
