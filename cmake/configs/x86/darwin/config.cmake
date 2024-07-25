set(WT_ARCH "x86" CACHE STRING "")
set(WT_OS "darwin" CACHE STRING "")
set(WT_POSIX ON CACHE BOOL "")

# Enable x86 SIMD instrinsics when available.
CHECK_INCLUDE_FILE("x86intrin.h" has_x86intrin)
if(has_x86intrin)
    add_cmake_flag(CMAKE_C_FLAGS -DHAVE_X86INTRIN_H)
endif()
unset(has_x86intrin CACHE)

# Disable cppsuite as it only runs on linux.
set(ENABLE_CPPSUITE 0)
