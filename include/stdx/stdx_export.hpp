#pragma once

#if defined(_WIN32)

// If we are building this library as a shared (DLL)
#  if defined(STDX_BUILD_SHARED)
     // Exporting symbols
#    define STDX_API __declspec(dllexport)
#  elif defined(STDX_USE_SHARED)
     // Importing symbols
#    define STDX_API __declspec(dllimport)
#  else
     // Building/using a static library, or no special instructions
#    define STDX_API
#  endif

#elif defined(__GNUC__) || defined(__clang__)

// If building as a shared library on Linux/macOS:
#  if defined(STDX_BUILD_SHARED)
#    define STDX_API __attribute__((visibility("default")))
#  else
#    define STDX_API
#  endif

#else
// Fallback for other compilers or platforms
#  define STDX_API
#endif