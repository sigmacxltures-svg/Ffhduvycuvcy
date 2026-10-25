cmake_minimum_required(VERSION 3.22)
project(NullClient)

# arm64-v8a only guard
if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "NullClient: arm64-v8a only, got '${CMAKE_ANDROID_ARCH_ABI}'")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    -O2
    -fvisibility=hidden
    -fno-rtti
    -fno-exceptions
    -march=armv8-a
    -Wall -Wextra -Wno-unused-parameter
)

add_library(NullClient SHARED src/main.cpp)

target_include_directories(NullClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_library(log-lib    log)
find_library(android-lib android)
find_library(dl-lib     dl)
find_library(glesv3-lib GLESv3)

target_link_libraries(NullClient
    ${log-lib} ${android-lib} ${dl-lib} ${glesv3-lib}
)

set_target_properties(NullClient PROPERTIES
    OUTPUT_NAME "NullClient"
    PREFIX      "lib"
    SUFFIX      ".so"
    CXX_VISIBILITY_PRESET hidden
    LINK_FLAGS "-Wl,--strip-debug -Wl,--exclude-libs,ALL"
)

message(STATUS "NullClient: configured → libNullClient.so (arm64-v8a)")
