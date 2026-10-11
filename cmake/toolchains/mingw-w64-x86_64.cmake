# Cross-compile for Windows x64 from WSL2 with Debian's MinGW-w64 (GCC 12, POSIX threads).
# The compilers are named explicitly so the build never depends on update-alternatives.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CRISPGUI_MINGW_TRIPLET x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${CRISPGUI_MINGW_TRIPLET}-gcc-posix)
set(CMAKE_CXX_COMPILER ${CRISPGUI_MINGW_TRIPLET}-g++-posix)
set(CMAKE_RC_COMPILER ${CRISPGUI_MINGW_TRIPLET}-windres)

# Debian 12's MinGW-w64 10.0 headers lack this Windows 10 1803 SDK constant, which EUI-NEO's
# frame pacing passes to CreateWaitableTimerExW (core/app/frame_pacing.h). 0x00000002 is the
# Windows SDK value; on older Windows the call fails and EUI-NEO falls back to a normal timer.
string(APPEND CMAKE_CXX_FLAGS_INIT " -DCREATE_WAITABLE_TIMER_HIGH_RESOLUTION=0x00000002")

# Search target headers and libraries only in the MinGW sysroot; run host programs.
set(CMAKE_FIND_ROOT_PATH /usr/${CRISPGUI_MINGW_TRIPLET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
