add_executable(example_01_bounce 01-bouncing-spheres.cpp)
target_link_libraries(example_01_bounce PRIVATE physics_core physics_render raylib)
target_include_directories(example_01_bounce PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(example_02_stack 02-box-stack.cpp)
target_link_libraries(example_02_stack PRIVATE physics_core physics_render raylib)
target_include_directories(example_02_stack PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(example_03_ramp 03-ramp-roll.cpp)
target_link_libraries(example_03_ramp PRIVATE physics_core physics_render raylib)
target_include_directories(example_03_ramp PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(example_04_gyro 04-gyroscope.cpp)
target_link_libraries(example_04_gyro PRIVATE physics_core physics_render raylib)
target_include_directories(example_04_gyro PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(example_05_stress 05-stress-test.cpp)
target_link_libraries(example_05_stress PRIVATE physics_core physics_render raylib)
target_include_directories(example_05_stress PRIVATE ${CMAKE_SOURCE_DIR}/src)

message(STATUS "Examples: use 'make example_01_bounce' etc. to build")
