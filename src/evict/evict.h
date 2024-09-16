/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#pragma once
// #include "evict_private.h"

#define WT_EVICT_PRESSURE_THRESHOLD 0.95
#define WT_EVICT_SCORE_CUTOFF 10
#define WT_EVICT_SCORE_MAX 100

struct __wt_evict {
    WT_EVICT_PRIV *priv;
    wt_shared volatile uint64_t eviction_progress; /* Eviction progress count */

    uint64_t app_waits;  /* User threads waited for cache */
    uint64_t app_evicts; /* Pages evicted by user threads */

    wt_shared uint64_t evict_max_page_size; /* Largest page seen at eviction */
    wt_shared uint64_t evict_max_ms;        /* Longest milliseconds spent at a single eviction */
    uint64_t reentry_hs_eviction_ms;        /* Total milliseconds spent inside a nested eviction */

    uint64_t evict_pass_gen; /* Number of eviction passes */

    /*
     * Eviction threshold percentages use double type to allow for specifying percentages less than
     * one.
     */
    wt_shared double eviction_dirty_target;  /* Percent to allow dirty */
    wt_shared double eviction_dirty_trigger; /* Percent to trigger dirty eviction */
    double eviction_target;                  /* Percent to end eviction */
    double eviction_trigger;                 /* Percent to trigger eviction */

    double eviction_checkpoint_target; /* Percent to reduce dirty to during checkpoint scrubs */
    wt_shared double eviction_scrub_target; /* Current scrub target */

    /*
     * Eviction threshold percentages use double type to allow for specifying percentages less than
     * one.
     */
    double eviction_updates_target;  /* Percent to allow for updates */
    double eviction_updates_trigger; /* Percent of updates to trigger eviction */
    /*
     * Score of how aggressive eviction should be about selecting eviction candidates. If eviction
     * is struggling to make progress, this score rises (up to a maximum of WT_EVICT_SCORE_MAX), at
     * which point the cache is "stuck" and transactions will be rolled back.
     */
    wt_shared uint32_t evict_aggressive_score;

    /*
     * Read information.
     */
    uint64_t read_gen;        /* Current page read generation */
    uint64_t read_gen_oldest; /* Oldest read generation the eviction
                               * server saw in its last queue load */
    /*
     * Pass interrupt counter.
     */
    wt_shared volatile uint32_t pass_intr; /* Interrupt eviction pass. */

    WT_DATA_HANDLE *walk_tree; /* LRU walk current tree */

/*
 * Flags.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_CACHE_EVICT_CLEAN 0x001u        /* Evict clean pages */
#define WT_CACHE_EVICT_CLEAN_HARD 0x002u   /* Clean % blocking app threads */
#define WT_CACHE_EVICT_DIRTY 0x004u        /* Evict dirty pages */
#define WT_CACHE_EVICT_DIRTY_HARD 0x008u   /* Dirty % blocking app threads */
#define WT_CACHE_EVICT_NOKEEP 0x010u       /* Don't add read pages to cache */
#define WT_CACHE_EVICT_SCRUB 0x020u        /* Scrub dirty pages */
#define WT_CACHE_EVICT_UPDATES 0x040u      /* Evict pages with updates */
#define WT_CACHE_EVICT_UPDATES_HARD 0x080u /* Update % blocking app threads */
#define WT_CACHE_EVICT_URGENT 0x100u       /* Pages are in the urgent queue */
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
#define WT_CACHE_EVICT_ALL (WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_DIRTY | WT_CACHE_EVICT_UPDATES)
#define WT_CACHE_EVICT_HARD \
    (WT_CACHE_EVICT_CLEAN_HARD | WT_CACHE_EVICT_DIRTY_HARD | WT_CACHE_EVICT_UPDATES_HARD)
    uint32_t flags;
};

/* Flags used with __wt_evict */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_EVICT_CALL_CLOSING 0x1u  /* Closing connection or tree */
#define WT_EVICT_CALL_NO_SPLIT 0x2u /* Splits not allowed */
#define WT_EVICT_CALL_URGENT 0x4u   /* Urgent eviction */
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

/* DO NOT EDIT: automatically built by prototypes.py: BEGIN */

extern bool __wt_page_evict_urgent(WT_SESSION_IMPL *session, WT_REF *ref)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_cache_eviction_worker(WT_SESSION_IMPL *session, bool busy, bool readonly,
  double pct_full) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_evict(WT_SESSION_IMPL *session, WT_REF *ref, WT_REF_STATE previous_state,
  uint32_t flags) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_evict_file(WT_SESSION_IMPL *session, WT_CACHE_OP syncop)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_evict_file_exclusive_on(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_evict_threads_create(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_evict_threads_destroy(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_eviction_config(WT_SESSION_IMPL *session, const char *cfg[], bool reconfig)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_eviction_create(WT_SESSION_IMPL *session, const char *cfg[])
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_eviction_destroy(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_verbose_dump_cache(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern void __wt_curstat_cache_walk(WT_SESSION_IMPL *session);
extern void __wt_evict_file_exclusive_off(WT_SESSION_IMPL *session);
extern void __wt_evict_priority_clear(WT_SESSION_IMPL *session);
extern void __wt_evict_priority_set(WT_SESSION_IMPL *session, uint64_t v);
extern void __wt_evict_server_wake(WT_SESSION_IMPL *session);
extern void __wt_eviction_stats_update(WT_SESSION_IMPL *session);

#ifdef HAVE_UNITTEST

#endif

/* DO NOT EDIT: automatically built by prototypes.py: END */
