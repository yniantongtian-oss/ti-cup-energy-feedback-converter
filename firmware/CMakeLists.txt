cmake_minimum_required(VERSION 3.16)
project(energy_feedback_converter_core C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(converter_core src/converter.c)
target_include_directories(converter_core PUBLIC include)
if(NOT MSVC)
    target_compile_options(converter_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(converter_demo src/demo.c)
target_link_libraries(converter_demo PRIVATE converter_core m)

include(CTest)
if(BUILD_TESTING)
    add_executable(converter_tests tests/test_converter.c)
    target_link_libraries(converter_tests PRIVATE converter_core m)
    add_test(NAME converter_tests COMMAND converter_tests)
endif()
