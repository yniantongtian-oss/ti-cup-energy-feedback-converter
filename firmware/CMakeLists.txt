cmake_minimum_required(VERSION 3.16)
project(energy_feedback_converter_core C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CONVERTER_ENABLE_COVERAGE "Instrument the host library with gcov" OFF)

add_library(converter_core
    src/converter.c
    src/converter_runtime.c
    src/pid.c
    src/pll.c
    src/protection.c
)
target_include_directories(converter_core PUBLIC include)
if(MSVC)
    target_compile_options(converter_core PRIVATE /W4 /WX)
    set(PLATFORM_MATH_LIBRARY "")
else()
    # Keep multiply-add in separate roundings. Clang contracts by default and
    # that changes calibrated ADC values in the last bits.
    target_compile_options(converter_core PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
    set(PLATFORM_MATH_LIBRARY m)
endif()

if(CONVERTER_ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "CONVERTER_ENABLE_COVERAGE requires GCC")
    endif()
    target_compile_options(converter_core PRIVATE -O0 -g --coverage)
    target_link_options(converter_core PUBLIC --coverage)
endif()

add_executable(converter_demo src/demo.c)
target_link_libraries(converter_demo PRIVATE converter_core ${PLATFORM_MATH_LIBRARY})

include(CTest)
if(BUILD_TESTING)
    function(add_converter_test name source)
        add_executable(${name} ${source})
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_link_libraries(${name} PRIVATE converter_core ${PLATFORM_MATH_LIBRARY})
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /WX /UNDEBUG)
        else()
            target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_converter_test(converter_tests tests/test_converter.c)
    add_converter_test(converter_runtime_tests tests/test_runtime.c)
    add_converter_test(pid_tests tests/test_pid.c)
    add_converter_test(pll_tests tests/test_pll.c)
    add_converter_test(protection_tests tests/test_protection.c)
endif()
