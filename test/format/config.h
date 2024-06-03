/* DO NOT EDIT: automatically built by format/config.sh. */

#pragma once

#define C_TYPE_MATCH(cp, type)                                                                    \
    (!F_ISSET(cp, (C_TYPE_FIX | C_TYPE_ROW | C_TYPE_VAR)) ||                                      \
      ((type) == FIX && F_ISSET(cp, C_TYPE_FIX)) || ((type) == ROW && F_ISSET(cp, C_TYPE_ROW)) || \
      ((type) == VAR && F_ISSET(cp, C_TYPE_VAR)))

typedef struct {
    const char *name; /* Configuration item */
    const char *desc; /* Configuration description */

#define C_BOOL 0x001u        /* Boolean (true if roll of 1-to-100 is <= CONFIG->min) */
#define C_IGNORE 0x002u      /* Not a simple randomization, configured specially */
#define C_STRING 0x004u      /* String (rather than integral) */
#define C_TABLE 0x008u       /* Value is per table, not global */
#define C_TYPE_FIX 0x010u    /* Value is only relevant to FLCS */
#define C_TYPE_LSM 0x020u    /* Value is only relevant to LSM */
#define C_TYPE_ROW 0x040u    /* Value is only relevant to RS */
#define C_TYPE_VAR 0x080u    /* Value is only relevant to VLCS */
#define C_ZERO_NOTSET 0x100u /* Ignore zero values */
    uint32_t flags;

    uint32_t min;     /* Minimum value */
    uint32_t maxrand; /* Maximum value randomly chosen */
    uint32_t maxset;  /* Maximum value explicitly set */

    u_int off; /* Value offset */
} CONFIG;

#define V_MAX_TABLES_CONFIG WT_THOUSAND

#define V_GLOBAL_ASSERT_READ_TIMESTAMP 0
#define V_GLOBAL_BACKGROUND_COMPACT 1
#define V_GLOBAL_BACKGROUND_COMPACT_FREE_SPACE_TARGET 2
#define V_GLOBAL_BACKUP 3
#define V_GLOBAL_BACKUP_INCREMENTAL 4
#define V_GLOBAL_BACKUP_INCR_GRANULARITY 5
#define V_GLOBAL_BLOCK_CACHE 6
#define V_GLOBAL_BLOCK_CACHE_CACHE_ON_CHECKPOINT 7
#define V_GLOBAL_BLOCK_CACHE_CACHE_ON_WRITES 8
#define V_GLOBAL_BLOCK_CACHE_SIZE 9
#define V_TABLE_BTREE_BITCNT 10
#define V_TABLE_BTREE_COMPRESSION 11
#define V_TABLE_BTREE_DICTIONARY 12
#define V_TABLE_BTREE_INTERNAL_KEY_TRUNCATION 13
#define V_TABLE_BTREE_INTERNAL_PAGE_MAX 14
#define V_TABLE_BTREE_KEY_MAX 15
#define V_TABLE_BTREE_KEY_MIN 16
#define V_TABLE_BTREE_LEAF_PAGE_MAX 17
#define V_TABLE_BTREE_MEMORY_PAGE_MAX 18
#define V_TABLE_BTREE_PREFIX_LEN 19
#define V_TABLE_BTREE_PREFIX_COMPRESSION 20
#define V_TABLE_BTREE_PREFIX_COMPRESSION_MIN 21
#define V_TABLE_BTREE_REPEAT_DATA_PCT 22
#define V_TABLE_BTREE_REVERSE 23
#define V_TABLE_BTREE_SPLIT_PCT 24
#define V_TABLE_BTREE_VALUE_MAX 25
#define V_TABLE_BTREE_VALUE_MIN 26
#define V_GLOBAL_CACHE 28
#define V_GLOBAL_CACHE_EVICT_MAX 29
#define V_GLOBAL_CACHE_EVICTION_DIRTY_TARGET 30
#define V_GLOBAL_CACHE_EVICTION_DIRTY_TRIGGER 31
#define V_GLOBAL_CACHE_MINIMUM 32
#define V_GLOBAL_CHECKPOINT 33
#define V_GLOBAL_CHECKPOINT_LOG_SIZE 34
#define V_GLOBAL_CHECKPOINT_WAIT 35
#define V_GLOBAL_CHUNK_CACHE 36
#define V_GLOBAL_CHUNK_CACHE_CAPACITY 37
#define V_GLOBAL_CHUNK_CACHE_CHUNK_SIZE 38
#define V_GLOBAL_CHUNK_CACHE_STORAGE_PATH 39
#define V_GLOBAL_CHUNK_CACHE_TYPE 40
#define V_GLOBAL_COMPACT_FREE_SPACE_TARGET 41
#define V_GLOBAL_DEBUG_CHECKPOINT_RETENTION 42
#define V_GLOBAL_DEBUG_CURSOR_REPOSITION 43
#define V_GLOBAL_DEBUG_EVICTION 44
#define V_GLOBAL_DEBUG_LOG_RETENTION 45
#define V_GLOBAL_DEBUG_REALLOC_EXACT 46
#define V_GLOBAL_DEBUG_REALLOC_MALLOC 47
#define V_GLOBAL_DEBUG_SLOW_CHECKPOINT 48
#define V_GLOBAL_DEBUG_TABLE_LOGGING 49
#define V_GLOBAL_DEBUG_UPDATE_RESTORE_EVICT 50
#define V_TABLE_DISK_CHECKSUM 51
#define V_GLOBAL_DISK_DATA_EXTEND 52
#define V_GLOBAL_DISK_ENCRYPTION 54
#define V_TABLE_DISK_FIRSTFIT 55
#define V_GLOBAL_DISK_MMAP 56
#define V_GLOBAL_DISK_MMAP_ALL 57
#define V_GLOBAL_FILE_MANAGER_CLOSE_HANDLE_MINIMUM 58
#define V_GLOBAL_FILE_MANAGER_CLOSE_IDLE_TIME 59
#define V_GLOBAL_FILE_MANAGER_CLOSE_SCAN_INTERVAL 60
#define V_GLOBAL_FORMAT_ABORT 61
#define V_GLOBAL_FORMAT_INDEPENDENT_THREAD_RNG 62
#define V_GLOBAL_FORMAT_MAJOR_TIMEOUT 63
#define V_GLOBAL_IMPORT 64
#define V_GLOBAL_LOGGING 65
#define V_GLOBAL_LOGGING_COMPRESSION 66
#define V_GLOBAL_LOGGING_FILE_MAX 67
#define V_GLOBAL_LOGGING_PREALLOC 68
#define V_GLOBAL_LOGGING_REMOVE 69
#define V_TABLE_LSM_AUTO_THROTTLE 70
#define V_TABLE_LSM_BLOOM 71
#define V_TABLE_LSM_BLOOM_BIT_COUNT 72
#define V_TABLE_LSM_BLOOM_HASH_COUNT 73
#define V_TABLE_LSM_BLOOM_OLDEST 74
#define V_TABLE_LSM_CHUNK_SIZE 75
#define V_TABLE_LSM_MERGE_MAX 76
#define V_GLOBAL_LSM_WORKER_THREADS 77
#define V_GLOBAL_OPS_ALTER 78
#define V_GLOBAL_OPS_COMPACTION 79
#define V_GLOBAL_OPS_HS_CURSOR 80
#define V_TABLE_OPS_PARETO 81
#define V_TABLE_OPS_PARETO_SKEW 82
#define V_TABLE_OPS_PCT_DELETE 83
#define V_TABLE_OPS_PCT_INSERT 84
#define V_TABLE_OPS_PCT_MODIFY 85
#define V_TABLE_OPS_PCT_READ 86
#define V_TABLE_OPS_PCT_WRITE 87
#define V_GLOBAL_OPS_BOUND_CURSOR 88
#define V_GLOBAL_OPS_PREPARE 89
#define V_GLOBAL_OPS_RANDOM_CURSOR 90
#define V_GLOBAL_OPS_SALVAGE 91
#define V_GLOBAL_OPS_THROTTLE 92
#define V_GLOBAL_OPS_THROTTLE_SLEEP_US 93
#define V_TABLE_OPS_TRUNCATE 94
#define V_GLOBAL_OPS_VERIFY 95
#define V_GLOBAL_PREFETCH 96
#define V_GLOBAL_QUIET 97
#define V_GLOBAL_RANDOM_DATA_SEED 98
#define V_GLOBAL_RANDOM_EXTRA_SEED 99
#define V_GLOBAL_RUNS_IN_MEMORY 100
#define V_TABLE_RUNS_MIRROR 101
#define V_GLOBAL_RUNS_OPS 102
#define V_GLOBAL_RUNS_PREDICTABLE_REPLAY 103
#define V_TABLE_RUNS_ROWS 104
#define V_TABLE_RUNS_SOURCE 105
#define V_GLOBAL_RUNS_TABLES 106
#define V_GLOBAL_RUNS_THREADS 107
#define V_GLOBAL_RUNS_TIMER 108
#define V_TABLE_RUNS_TYPE 109
#define V_GLOBAL_RUNS_VERIFY_FAILURE_DUMP 110
#define V_GLOBAL_STATISTICS_MODE 111
#define V_GLOBAL_STATISTICS_LOG_SOURCES 112
#define V_GLOBAL_STRESS_AGGRESSIVE_STASH_FREE 113
#define V_GLOBAL_STRESS_AGGRESSIVE_SWEEP 114
#define V_GLOBAL_STRESS_CHECKPOINT 115
#define V_GLOBAL_STRESS_CHECKPOINT_EVICT_PAGE 116
#define V_GLOBAL_STRESS_CHECKPOINT_PREPARE 117
#define V_GLOBAL_STRESS_COMPACT_SLOW 118
#define V_GLOBAL_STRESS_EVICT_REPOSITION 119
#define V_GLOBAL_STRESS_FAILPOINT_EVICTION_SPLIT 120
#define V_GLOBAL_STRESS_FAILPOINT_HS_DELETE_KEY_FROM_TS 121
#define V_GLOBAL_STRESS_HS_CHECKPOINT_DELAY 122
#define V_GLOBAL_STRESS_HS_SEARCH 123
#define V_GLOBAL_STRESS_HS_SWEEP 124
#define V_GLOBAL_STRESS_PREFETCH_DELAY 125
#define V_GLOBAL_STRESS_PREPARE_RESOLUTION_1 126
#define V_GLOBAL_STRESS_SLEEP_BEFORE_READ_OVERFLOW_ONPAGE 127
#define V_GLOBAL_STRESS_SPLIT_1 128
#define V_GLOBAL_STRESS_SPLIT_2 129
#define V_GLOBAL_STRESS_SPLIT_3 130
#define V_GLOBAL_STRESS_SPLIT_4 131
#define V_GLOBAL_STRESS_SPLIT_5 132
#define V_GLOBAL_STRESS_SPLIT_6 133
#define V_GLOBAL_STRESS_SPLIT_7 134
#define V_GLOBAL_STRESS_SPLIT_8 135
#define V_GLOBAL_TIERED_STORAGE_FLUSH_FREQUENCY 136
#define V_GLOBAL_TIERED_STORAGE_STORAGE_SOURCE 137
#define V_GLOBAL_TRANSACTION_IMPLICIT 138
#define V_GLOBAL_TRANSACTION_OPERATION_TIMEOUT_MS 139
#define V_GLOBAL_TRANSACTION_TIMESTAMPS 140
#define V_GLOBAL_WIREDTIGER_CONFIG 141
#define V_GLOBAL_WIREDTIGER_RWLOCK 142
#define V_GLOBAL_WIREDTIGER_LEAK_MEMORY 143

#define V_ELEMENT_COUNT 144
