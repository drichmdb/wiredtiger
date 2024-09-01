/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 *
 * This file contains inline functions that are part of the public API to the eviction
 * module.
 */

#pragma once

/*
 * __wt_cache_aggressive --
 *     Indicate if the cache is operating in aggressive mode.
 */
static WT_INLINE bool
__wt_cache_aggressive(WT_SESSION_IMPL *session)
{
    return (
      __wt_atomic_load32(&S2C(session)->evict->evict_aggressive_score) >= WT_EVICT_SCORE_CUTOFF);
}

/*
 * __wt_cache_read_gen --
 *     Get the current read generation number.
 */
static WT_INLINE uint64_t
__wt_cache_read_gen(WT_SESSION_IMPL *session)
{
    return (__wt_atomic_load64(&S2C(session)->evict->read_gen));
}

/*
 * __wt_cache_read_gen_incr --
 *     Increment the current read generation number.
 */
static WT_INLINE void
__wt_cache_read_gen_incr(WT_SESSION_IMPL *session)
{
    (void)__wt_atomic_add64(&S2C(session)->evict->read_gen, 1);
}

/*
 * __wt_cache_read_gen_new --
 *     Get the read generation for a new page in memory.
 */
static WT_INLINE void
__wt_cache_read_gen_new(WT_SESSION_IMPL *session, WT_PAGE *page)
{
    WT_EVICT *evict;

    evict = S2C(session)->evict;
    __wt_atomic_store64(
      &page->read_gen, (__wt_cache_read_gen(session) + evict->read_gen_oldest) / 2);
}

/*
 * __wt_cache_stuck --
 *     Indicate if the cache is stuck (i.e., not making progress).
 */
static WT_INLINE bool
__wt_cache_stuck(WT_SESSION_IMPL *session)
{
    WT_EVICT *evict;
    uint32_t tmp_evict_aggressive_score;

    evict = S2C(session)->evict;
    tmp_evict_aggressive_score = __wt_atomic_load32(&evict->evict_aggressive_score);
    WT_ASSERT(session, tmp_evict_aggressive_score <= WT_EVICT_SCORE_MAX);
    return (
      tmp_evict_aggressive_score == WT_EVICT_SCORE_MAX && F_ISSET(evict, WT_CACHE_EVICT_HARD));
}

/*
 * __wt_page_evict_soon --
 *     Set a page to be evicted as soon as possible.
 */
static WT_INLINE void
__wt_page_evict_soon(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_UNUSED(session);

    __wt_atomic_store64(&ref->page->read_gen, WT_READGEN_OLDEST);
}

/*
 * __wt_page_dirty_and_evict_soon --
 *     Mark a page dirty and set it to be evicted as soon as possible.
 */
static WT_INLINE int
__wt_page_dirty_and_evict_soon(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_RET(__wt_page_modify_init(session, ref->page));
    __wt_page_modify_set(session, ref->page);
    __wt_page_evict_soon(session, ref);

    return (0);
}

/*
 * __wt_eviction_clean_pressure --
 *     Return true if clean cache is stressed and will soon require application threads to evict
 *     content.
 */
static WT_INLINE bool
__wt_eviction_clean_pressure(WT_SESSION_IMPL *session)
{
    WT_EVICT *evict;
    double pct_full;

    evict = S2C(session)->evict;
    pct_full = 0;

    /* Eviction should be done if we hit the eviction clean trigger or come close to hitting it. */
    if (__wt_eviction_clean_needed(session, &pct_full))
        return (true);
    if (pct_full > evict->eviction_target &&
      pct_full >= WT_EVICT_PRESSURE_THRESHOLD * evict->eviction_trigger)
        return (true);
    return (false);
}

/*
 * __wt_eviction_clean_needed --
 *     Return if an application thread should do eviction due to the total volume of data in cache.
 */
static WT_INLINE bool
__wt_eviction_clean_needed(WT_SESSION_IMPL *session, double *pct_fullp)
{
    WT_EVICT *evict;
    uint64_t bytes_inuse, bytes_max;

    evict = S2C(session)->evict;

    /*
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_max = S2C(session)->cache_size + 1;
    bytes_inuse = __wt_cache_bytes_inuse(S2C(session)->cache);

    if (pct_fullp != NULL)
        *pct_fullp = ((100.0 * bytes_inuse) / bytes_max);

    return (bytes_inuse > (evict->eviction_trigger * bytes_max) / 100);
}

/*
 * __wt_eviction_dirty_target --
 *     Return the effective dirty target (including checkpoint scrubbing).
 */
static WT_INLINE double
__wt_eviction_dirty_target(WT_SESSION_IMPL *session)
{
    WT_EVICT *evict;
    double dirty_target, scrub_target;

    evict = S2C(session)->evict;

    dirty_target = __wt_read_shared_double(&evict->eviction_dirty_target);
    scrub_target = __wt_read_shared_double(&evict->eviction_scrub_target);

    return (scrub_target > 0 && scrub_target < dirty_target ? scrub_target : dirty_target);
}

/*
 * __wt_eviction_dirty_needed --
 *     Return if an application thread should do eviction due to the total volume of dirty data in
 *     cache.
 */
static WT_INLINE bool
__wt_eviction_dirty_needed(WT_SESSION_IMPL *session, double *pct_fullp)
{
    WT_EVICT *evict;
    uint64_t bytes_dirty, bytes_max;

    evict = S2C(session)->evict;

    /*
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_dirty = __wt_cache_dirty_leaf_inuse(S2C(session)->cache);
    bytes_max = S2C(session)->cache_size + 1;

    if (pct_fullp != NULL)
        *pct_fullp = (100.0 * bytes_dirty) / bytes_max;

    return (bytes_dirty > (uint64_t)(evict->eviction_dirty_trigger * bytes_max) / 100);
}

/*
 * __wt_eviction_updates_needed --
 *     Return if an application thread should do eviction due to the total volume of updates in
 *     cache.
 */
static WT_INLINE bool
__wt_eviction_updates_needed(WT_SESSION_IMPL *session, double *pct_fullp)
{
    WT_EVICT *evict;
    uint64_t bytes_max, bytes_updates;

    evict = S2C(session)->evict;

    /*
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_max = S2C(session)->cache_size + 1;
    bytes_updates = __wt_cache_bytes_updates(S2C(session)->cache);

    if (pct_fullp != NULL)
        *pct_fullp = (100.0 * bytes_updates) / bytes_max;

    return (bytes_updates > (uint64_t)(evict->eviction_updates_trigger * bytes_max) / 100);
}

/*
 * __wt_btree_dominating_cache --
 *     Return if a single btree is occupying at least half of any of our target's cache usage.
 */
static WT_INLINE bool
__wt_btree_dominating_cache(WT_SESSION_IMPL *session, WT_BTREE *btree)
{
    WT_EVICT *evict;
    uint64_t bytes_dirty;
    uint64_t bytes_max;

    evict = S2C(session)->evict;
    bytes_max = S2C(session)->cache_size + 1;

    if (__wt_cache_bytes_plus_overhead(
          S2C(session)->cache, __wt_atomic_load64(&btree->bytes_inmem)) >
      (uint64_t)(0.5 * evict->eviction_target * bytes_max) / 100)
        return (true);

    bytes_dirty =
      __wt_atomic_load64(&btree->bytes_dirty_intl) + __wt_atomic_load64(&btree->bytes_dirty_leaf);
    if (__wt_cache_bytes_plus_overhead(S2C(session)->cache, bytes_dirty) >
      (uint64_t)(0.5 * evict->eviction_dirty_target * bytes_max) / 100)
        return (true);
    if (__wt_cache_bytes_plus_overhead(
          S2C(session)->cache, __wt_atomic_load64(&btree->bytes_updates)) >
      (uint64_t)(0.5 * evict->eviction_updates_target * bytes_max) / 100)
        return (true);

    return (false);
}

/*
 * __wt_eviction_needed --
 *     Return if an application thread should do eviction, and the cache full percentage as a
 *     side-effect.
 */
static WT_INLINE bool
__wt_eviction_needed(WT_SESSION_IMPL *session, bool busy, bool readonly, double *pct_fullp)
{
    WT_EVICT *evict;
    double pct_dirty, pct_full, pct_updates;
    bool clean_needed, dirty_needed, updates_needed;

    evict = S2C(session)->evict;

    /*
     * If the connection is closing we do not need eviction from an application thread. The eviction
     * subsystem is already closed.
     */
    if (F_ISSET(S2C(session), WT_CONN_CLOSING))
        return (false);

    clean_needed = __wt_eviction_clean_needed(session, &pct_full);
    if (readonly) {
        dirty_needed = updates_needed = false;
        pct_dirty = pct_updates = 0.0;
    } else {
        dirty_needed = __wt_eviction_dirty_needed(session, &pct_dirty);
        updates_needed = __wt_eviction_updates_needed(session, &pct_updates);
    }

    /*
     * Calculate the cache full percentage; anything over the trigger means we involve the
     * application thread.
     */
    if (pct_fullp != NULL)
        *pct_fullp = WT_MAX(0.0,
          100.0 -
            WT_MIN(
              WT_MIN(evict->eviction_trigger - pct_full, evict->eviction_dirty_trigger - pct_dirty),
              evict->eviction_updates_trigger - pct_updates));

    /*
     * Only check the dirty trigger when the session is not busy.
     *
     * In other words, once we are pinning resources, try to finish the operation as quickly as
     * possible without exceeding the cache size. The next transaction in this session will not be
     * able to start until the cache is under the limit.
     */
    return (clean_needed || updates_needed || (!busy && dirty_needed));
}

/*
 * __wt_cache_hs_dirty --
 *     Return if a major portion of the cache is dirty due to history store content.
 */
static WT_INLINE bool
__wt_cache_hs_dirty(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_EVICT *evict;
    uint64_t bytes_max;
    conn = S2C(session);
    evict = conn->evict;
    bytes_max = conn->cache_size;

    return (__wt_cache_bytes_plus_overhead(
              conn->cache, __wt_atomic_load64(&conn->cache->bytes_hs_dirty)) >=
      ((uint64_t)(evict->eviction_dirty_trigger * bytes_max) / 100));
}

/*
 * __wt_cache_eviction_check --
 *     Evict pages if the cache crosses its boundaries.
 */
static WT_INLINE int
__wt_cache_eviction_check(WT_SESSION_IMPL *session, bool busy, bool readonly, bool *didworkp)
{
    WT_BTREE *btree;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;
    double pct_full;

    if (didworkp != NULL)
        *didworkp = false;

    /* Eviction causes reconciliation. So don't evict if we can't reconcile */
    if (F_ISSET(session, WT_SESSION_NO_RECONCILE))
        return (0);

    /* If the transaction is prepared don't evict. */
    if (F_ISSET(session->txn, WT_TXN_PREPARE))
        return (0);

    /*
     * If the transaction is a checkpoint cursor transaction, don't try to evict. Because eviction
     * keeps the current transaction snapshot, and the snapshot in a checkpoint cursor transaction
     * can be (and likely is) very old, we won't be able to see anything current to evict and won't
     * be able to accomplish anything useful.
     */
    if (F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT))
        return (0);

    /*
     * If the current transaction is keeping the oldest ID pinned, it is in the middle of an
     * operation. This may prevent the oldest ID from moving forward, leading to deadlock, so only
     * evict what we can. Otherwise, we are at a transaction boundary and we can work harder to make
     * sure there is free space in the cache.
     */
    txn_global = &S2C(session)->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);
    busy = busy || __wt_atomic_loadv64(&txn_shared->id) != WT_TXN_NONE ||
      session->hazards.num_active > 0 ||
      (__wt_atomic_loadv64(&txn_shared->pinned_id) != WT_TXN_NONE &&
        __wt_atomic_loadv64(&txn_global->current) != __wt_atomic_loadv64(&txn_global->oldest_id));

    /*
     * LSM sets the "ignore cache size" flag when holding the LSM tree lock, in that case, or when
     * holding the handle list, schema or table locks (which can block checkpoints and eviction),
     * don't block the thread for eviction.
     */
    if (F_ISSET(session, WT_SESSION_IGNORE_CACHE_SIZE) ||
      FLD_ISSET(session->lock_flags,
        WT_SESSION_LOCKED_HANDLE_LIST | WT_SESSION_LOCKED_SCHEMA | WT_SESSION_LOCKED_TABLE))
        return (0);

    /* In memory configurations don't block when the cache is full. */
    if (F_ISSET(S2C(session), WT_CONN_IN_MEMORY))
        return (0);

    /*
     * Threads operating on cache-resident trees are ignored because they're not contributing to the
     * problem. We also don't block while reading metadata because we're likely to be holding some
     * other resources that could block checkpoints or eviction.
     */
    btree = S2BT_SAFE(session);
    if (btree != NULL && (F_ISSET(btree, WT_BTREE_IN_MEMORY) || WT_IS_METADATA(session->dhandle)))
        return (0);

    /* Check if eviction is needed. */
    if (!__wt_eviction_needed(session, busy, readonly, &pct_full))
        return (0);

    /*
     * Some callers (those waiting for slow operations), will sleep if there was no cache work to
     * do. After this point, let them skip the sleep.
     */
    if (didworkp != NULL)
        *didworkp = true;

    return (__wt_cache_eviction_worker(session, busy, readonly, pct_full));
}

/*
 * __wt_evict_page_init --
 *     Initialize eviction state for a newly created page.
 */
static WT_INLINE void
__wt_evict_page_init(WT_PAGE *page)
{
    __wt_atomic_store64(&page->read_gen, WT_READGEN_NOTSET);
}

/*
 * __wt_readgen_evict_soon --
 *     Return whether a read generation value makes a page eligible for immediate eviction. Read
 *     generations reserve a range of low numbers for special meanings and currently - with the
 *     exception of the generation not being set - these indicate the page may be evicted
 *     immediately.
 */
static WT_INLINE bool
__wt_readgen_evict_soon(uint64_t *read_gen)
{
    uint64_t gen;

    WT_READ_ONCE(gen, *read_gen);
    return (gen != WT_READGEN_NOTSET && gen < WT_READGEN_START_VALUE);
}

/*
 * __wt_evict_page_is_soon --
 *     Return whether the page is eligible for immediate eviction.
 */
static WT_INLINE bool
__wt_evict_page_is_soon(WT_PAGE *page)
{
    return (__wt_readgen_evict_soon(&page->read_gen));
}

/*
 * __wt_evict_copy_page_state --
 *     When creating a new page from an existing page, for example during split, initialize the read
 *     generation on the new page using the state of the original page.
 */
static WT_INLINE void
__wt_evict_copy_page_state(WT_PAGE *orig_page, WT_PAGE *new_page)
{
    uint64_t orig_read_gen;

    WT_READ_ONCE(orig_read_gen, orig_page->read_gen);

    /*
     * In the current use case, we are initializing/splitting the new page and it should be
     * impossible to have a race during the store. But to protect against future uses that violate
     * this assumption use an atomic store.
     */
    __wt_atomic_store64(&new_page->read_gen, orig_read_gen);
}
