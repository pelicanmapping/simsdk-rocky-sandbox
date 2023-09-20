cmake_minimum_required(VERSION 3.7)

# if using vcpkg, configure overlays
if (${CMAKE_TOOLCHAIN_FILE} MATCHES ".*vcpkg.cmake.*")
    message(STATUS "Building with vcpkg toolchain.")
    set(USING_VCPKG ON)
    set(VCPKG_OVERLAY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/ports)
endif()

project(simsdk-vk VERSION 1.0.0 LANGUAGES CXX C)

find_package(rocky CONFIG REQUIRED)
find_package(simCore CONFIG REQUIRED)
find_package(simData CONFIG REQUIRED)

# secondary dependencies
find_package(glslang CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

file(GLOB HEADERS *.h)
file(GLOB SOURCES *.cpp)

add_executable(simdemo ${SOURCES} ${HEADERS})

target_link_libraries(simdemo PUBLIC rocky::rocky rocky::rocky_vsg VSI::simCore VSI::simData)

install(TARGETS simdemo RUNTIME DESTINATION bin)
