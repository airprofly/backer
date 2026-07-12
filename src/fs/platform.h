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

// Cross‑platform stat timestamp field access.
//   Linux:   st_atim.tv_sec / st_atim.tv_nsec
//   macOS:   st_atimespec.tv_sec / st_atimespec.tv_nsec
#if defined(__APPLE__) && defined(__MACH__)
    #define BACKER_STAT_ATIME(st) (st).st_atimespec
    #define BACKER_STAT_MTIME(st) (st).st_mtimespec
    #define BACKER_STAT_CTIME(st) (st).st_ctimespec
#else
    #define BACKER_STAT_ATIME(st) (st).st_atim
    #define BACKER_STAT_MTIME(st) (st).st_mtim
    #define BACKER_STAT_CTIME(st) (st).st_ctim
#endif
