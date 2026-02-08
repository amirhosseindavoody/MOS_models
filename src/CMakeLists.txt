# Mini-SPICE source files

# Core library
set(MINISPICE_SOURCES
    stamp.cc
    device.cc
    circuit.cc
    parser.cc
    minispice.cc
)

add_library(minispice STATIC ${MINISPICE_SOURCES})
target_include_directories(minispice PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Main executable
add_executable(mini-spice main.cc)
target_link_libraries(mini-spice minispice)

# Unit tests
add_executable(stamp_test stamp_test.cc)
target_link_libraries(stamp_test minispice ${GTEST})
gtest_discover_tests(stamp_test)

add_executable(devices_test devices_test.cc)
target_link_libraries(devices_test minispice ${GTEST})
gtest_discover_tests(devices_test)

add_executable(circuit_test circuit_test.cc)
target_link_libraries(circuit_test minispice ${GTEST})
gtest_discover_tests(circuit_test)