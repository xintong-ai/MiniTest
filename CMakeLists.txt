# CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.
cmake_minimum_required (VERSION 2.8.11)
project (MiniTest)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(PhysX)
find_package(APEX)


include_directories(${PHYSX_INCLUDE_DIRS} ${APEX_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} MinimalTurbulence.cpp)
target_link_libraries(${PROJECT_NAME} ${PHYSX_LIBRARIES} ${APEX_LIBRARIES})