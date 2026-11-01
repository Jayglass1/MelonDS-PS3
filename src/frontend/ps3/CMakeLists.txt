# Host-side headless runner for the PS3 frontend code (see README.md).
# The PS3 executable itself is built with the Makefile in this directory.

option(PS3_FRONTEND_SINGLE_THREADED
       "Use the single-threaded sync backend (the one the PS3 build uses)" OFF)

add_executable(melonds-ps3-headless
    main_headless.cpp
    EmuCore.cpp
    Platform_PS3.cpp
)

if (PS3_FRONTEND_SINGLE_THREADED)
    target_sources(melonds-ps3-headless PRIVATE Sync_single.cpp)
else()
    target_sources(melonds-ps3-headless PRIVATE Sync_std.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(melonds-ps3-headless PRIVATE Threads::Threads)
endif()

target_include_directories(melonds-ps3-headless PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(melonds-ps3-headless PRIVATE core)
