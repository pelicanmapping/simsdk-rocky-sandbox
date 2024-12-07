cmake_minimum_required(VERSION 3.7)

# if using vcpkg, configure overlays
if (${CMAKE_TOOLCHAIN_FILE} MATCHES ".*vcpkg.cmake.*")
    message(STATUS "Building with vcpkg toolchain.")
    set(USING_VCPKG ON)
    #set(VCPKG_OVERLAY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/ports)
endif()

project(simsdk-vk VERSION 1.0.0 LANGUAGES CXX C)

# Locate the SIMDIS SDK

# Use an external script to load the SIMDIS SDK targets
include($ENV{SIMDIS_SDK_DIR}/share/ExternalSdkProject/CMakeModules/ImportSIMDIS_SDK.cmake)

find_package(rocky CONFIG REQUIRED)

file(GLOB HEADERS src/*.h)
file(GLOB SOURCES src/*.cpp)

add_executable(simdemo ${SOURCES} ${HEADERS})

target_link_libraries(simdemo PUBLIC rocky::rocky VSI::simCore VSI::simData)

install(TARGETS simdemo RUNTIME DESTINATION bin)
