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
add_executable(test_stamp test_stamp.cc)
target_link_libraries(test_stamp minispice ${GTEST})
gtest_discover_tests(test_stamp)

add_executable(test_devices test_devices.cc)
target_link_libraries(test_devices minispice ${GTEST})
gtest_discover_tests(test_devices)

add_executable(test_circuit test_circuit.cc)
target_link_libraries(test_circuit minispice ${GTEST})
gtest_discover_tests(test_circuit)