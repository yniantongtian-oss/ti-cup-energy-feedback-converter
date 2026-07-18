cmake_minimum_required(VERSION 3.16)
project(energy_feedback_converter_core C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(converter_core
    src/converter.c
    src/converter_runtime.c
)
target_include_directories(converter_core PUBLIC include)
if(MSVC)
    target_compile_options(converter_core PRIVATE /W4 /WX)
    set(PLATFORM_MATH_LIBRARY "")
else()
    target_compile_options(converter_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
    set(PLATFORM_MATH_LIBRARY m)
endif()

add_executable(converter_demo src/demo.c)
target_link_libraries(converter_demo PRIVATE converter_core ${PLATFORM_MATH_LIBRARY})

include(CTest)
if(BUILD_TESTING)
    add_executable(converter_tests tests/test_converter.c)
    target_link_libraries(converter_tests PRIVATE converter_core ${PLATFORM_MATH_LIBRARY})
    add_test(NAME converter_tests COMMAND converter_tests)

    add_executable(converter_runtime_tests tests/test_runtime.c)
    target_link_libraries(converter_runtime_tests PRIVATE converter_core ${PLATFORM_MATH_LIBRARY})
    add_test(NAME converter_runtime_tests COMMAND converter_runtime_tests)
endif()
