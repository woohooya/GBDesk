cmake_minimum_required(VERSION 3.22.1)
project(gbdesk_jni LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(MGBA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../" ABSOLUTE)

set(DISABLE_FRONTENDS ON CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(USE_DISCORD_RPC OFF CACHE BOOL "" FORCE)
set(USE_SQLITE3 OFF CACHE BOOL "" FORCE)
set(USE_LIBZIP OFF CACHE BOOL "" FORCE)
set(USE_MINIZIP OFF CACHE BOOL "" FORCE)
set(USE_FFMPEG OFF CACHE BOOL "" FORCE)
set(USE_EDITLINE OFF CACHE BOOL "" FORCE)
set(USE_GDB_STUB OFF CACHE BOOL "" FORCE)
set(BUILD_SUITE OFF CACHE BOOL "" FORCE)

add_subdirectory("${MGBA_ROOT}" mgba-build EXCLUDE_FROM_ALL)

find_package(oboe REQUIRED CONFIG)

add_library(gbdesk_jni SHARED
    jni_bridge.cpp
    android_video.cpp
    android_audio.cpp
)

target_include_directories(gbdesk_jni PRIVATE
    "${MGBA_ROOT}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/mgba-build/include"
)

get_target_property(MGBA_DEFS mgba COMPILE_DEFINITIONS)
if(MGBA_DEFS)
    target_compile_definitions(gbdesk_jni PRIVATE ${MGBA_DEFS})
endif()

target_link_libraries(gbdesk_jni PRIVATE
    mgba
    oboe::oboe
    android
    EGL
    GLESv3
    log
    z
)
