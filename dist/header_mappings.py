# Header files in include/ often are aligned with module up the src/ folder.
# This dict provided a mapping that is used in modularity_check.py
header_mappings = {
    "api.h": "conn",
    "bitstring_inline.h": "support",
    "bitstring.h": "support",
    "block_cache.h": "block_cache",
    "block_chunkcache.h": "block_cache",
    "block_inline.h": "block",
    "block.h": "block",
    "bloom.h": "bloom",
    "btmem.h": "btmem",
    "btree_cmp_inline.h": "btree",
    "btree_inline.h": "btree",
    "btree.h": "btree",
    "buf_inline.h": "include",
    "cache_inline.h": "evict",
    "cache.h": "evict",
    "capacity.h": "include",
    "cell_inline.h": "reconcile",
    "cell.h": "reconcile",
    "checkpoint.h": "btree",
    "column_inline.h": "btree",
    "compact.h": "btree",
    "conf_inline.h": "conf",
    "conf_keys.h": "conf",
    "conf.h": "conf",
    "config.h": "config",
    "connection.h": "conn", # TODO - double check these. Autocorrect catching me out >:(
    "ctype_inline.h": "include",
    "cursor_inline.h": "cursor",
    "cursor.h": "cursor",
    "dhandle.h": "btree",
    "dlh.h": "include",
    "error.h": "include",
    "futex.h": "os_common",
    "gcc.h": "include",
    "generation.h": "support",
    "hardware.h": "include",
    "hazard.h": "support",
    "intpack_inline.h": "packing",
    "log_inline.h": "log",
    "log.h": "log",
    "lsm.h": "lsm",
    "meta.h": "meta",
    "misc_inline.h": "include",
    "misc.h": "include",
    "msvc.h": "include",
    "mutex_inline.h": "support",
    "mutex.h": "support",
    "optrack.h": "optrack",
    "os_fhandle_inline.h": "os_common",
    "os_fs_inline.h": "os_common",
    "os_fstream_inline.h": "os_common",
    "os_windows.h": "os_common",
    "os.h": "os_common",
    "packing_inline.h": "packing",
    "posix.h": "os_common",
    "queue.h": "include",
    "reconcile_inline.h": "reconcile",
    "reconcile.h": "reconcile",
    "ref_inline.h": "btree",
    "rollback_to_stable.h": "rollback_to_stable",
    "schema.h": "schema",
    "serial_inline.h": "btree",
    "session.h": "session",
    "stat.h": "support",
    "str_inline.h": "support",
    "swap.h": "include",
    "thread_group.h": "support",
    "tiered.h": "tiered",
    "time_inline.h": "support",
    "timestamp_inline.h": "support",
    "timestamp.h": "support",
    "truncate.h": "txn",
    "txn_inline.h": "txn",
    "txn.h": "txn",
    "verbose.h": "support",
    "version.h": "include",
    "verify_build.h": "include",
}

# Forward declaration files aren't needed when building dependency graphs. Skip them.
skip_files = [
    "extern_posix.h", "extern_darwin.h", "extern_win.h", "extern.h", 
    "extern_linux.h", "wt_internal.h", "wiredtiger_ext.h"
]
