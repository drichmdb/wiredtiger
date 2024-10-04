project(reverse_collator C)

set(sources "reverse_collator.c")
add_library(wiredtiger_reverse_collator MODULE ${sources})

target_include_directories(
    wiredtiger_reverse_collator
    PRIVATE
        # FIXME-WT-13610 I'm not sure this is required for collators or compressors. I've only 
        # seen evidence we need this line in the storage_sources files. 
        # We should run a patch test with this line removed (for all collators/compressors) to confirm.
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_BINARY_DIR}/config
)
target_compile_options(
    wiredtiger_reverse_collator
    PRIVATE ${COMPILER_DIAGNOSTIC_C_FLAGS}
)
