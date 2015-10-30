cmake_minimum_required(VERSION 3.1.3)

# set some good warning flags
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(cxxFlags
        -std=c++11
        -fdiagnostics-show-option
        -Wall -Wextra -Wpedantic -Weverything -Wno-c++98-compat -Werror)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(cxxFlags
        -std=c++11
        -fdiagnostics-show-option
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion
        -Wno-comment)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # from: https://msdn.microsoft.com/en-us/library/thxezb7y.aspx
    # untested
    set(cxxFlags
        /Wall /WX)
else()
    # unknown compiler...
    set(cxxFlags "")
endif()

string(REPLACE ";" " " cxxFlags "${cxxFlags}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxxFlags}")

# a simple showcase of hyper_array's API
set(playground hyper_array_playground)
add_executable(${playground} src/playground.cpp)
target_include_directories(${playground} PRIVATE include)
set_property(TARGET ${playground} PROPERTY CXX_STANDARD 11)

# pragmatic testing using CATCH
file(GLOB test_files "test/*.cpp")
add_executable(tests "${test_files}")  # "test" is a reserved target name
set_property(TARGET tests PROPERTY CXX_STANDARD 11)
# operator<< for array_order conflicts with CATCH
target_compile_definitions(tests PRIVATE HYPER_ARRAY_CONFIG_Overload_Stream_Operator=0)
