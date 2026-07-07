#pragma once

// Platform detection for backer project.
// The project primarily targets Linux, but we provide graceful
// fallbacks when building on other platforms.

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define BACKER_PLATFORM_POSIX 1
#else
    #define BACKER_PLATFORM_POSIX 0
#endif

// MSYS2 / MinGW on Windows defines _WIN32 but may have some POSIX headers.
// Treat it as non-POSIX for our purposes (missing sys/sysmacros.h, etc.)
#if defined(_WIN32) || defined(_MSC_VER)
    #define BACKER_PLATFORM_WINDOWS 1
#else
    #define BACKER_PLATFORM_WINDOWS 0
#endif
