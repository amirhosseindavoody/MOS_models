cmake_minimum_required(VERSION 3.30)

project(mini-spice LANGUAGES CXX)

include(CTest)
include(GoogleTest)
include(FetchContent)

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example when
# compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0) # Replace with the desired version tag
FetchContent_MakeAvailable(googletest)

set(GTEST GTest::gtest_main GTest::gmock_main)

add_subdirectory(src)
