cmake_minimum_required(VERSION 3.14)  # 3.14+ is needed for FetchContent_MakeAvailable
project(KM8 VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # plain C++17, not the GNU dialect

# Turn on the common warnings so mistakes show up at compile time.
function(km8_set_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# km8_core: everything except main(). The command-line program and the tests
# both link against it, so they always exercise exactly the same code.
# ---------------------------------------------------------------------------
add_library(km8_core
    src/cpu.cpp
)
target_include_directories(km8_core PUBLIC include)
km8_set_warnings(km8_core)

# ---------------------------------------------------------------------------
# km8: the command-line program
# ---------------------------------------------------------------------------
add_executable(km8 src/main.cpp)
target_link_libraries(km8 PRIVATE km8_core)
km8_set_warnings(km8)

# ---------------------------------------------------------------------------
# km8_tests: unit tests using Catch2 v3, downloaded at configure time
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(km8_tests
    tests/test_cpu_core.cpp
    tests/test_alu_and_jumps.cpp
)
target_link_libraries(km8_tests PRIVATE km8_core Catch2::Catch2WithMain)
km8_set_warnings(km8_tests)

# Register every Catch2 test case with CTest, so "ctest" runs them all.
enable_testing()
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(km8_tests)
