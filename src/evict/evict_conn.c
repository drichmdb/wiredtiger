/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#define WT_CONFIG_DEBUG(session, fmt, ...)                                 \
    if (FLD_ISSET(S2C(session)->debug_flags, WT_CONN_DEBUG_CONFIGURATION)) \
        __wt_verbose_warning(session, WT_VERB_CONFIGURATION, fmt, __VA_ARGS__);

/*
 * __evict_config_abs_to_pct --
 *     Evict configuration values can be either a percentage or an absolute size, this function
 *     converts an absolute size to a percentage.
 */
static WT_INLINE int
__evict_config_abs_to_pct(
  WT_SESSION_IMPL *session, double *param, const char *param_name, bool shared)
{
    WT_CONNECTION_IMPL *conn;
    double input;

    conn = S2C(session);

    WT_ASSERT(session, param != NULL);
    input = *param;

    /*
     * Anything above 100 is an absolute value; convert it to percentage.
     */
    if (input > 100.0) {
        /*
         * In a shared cache configuration the cache size changes regularly. Therefore, we require a
         * percentage setting and do not allow an absolute size setting.
         */
        if (shared)
            WT_RET_MSG(session, EINVAL,
              "Shared cache configuration requires a percentage value for %s", param_name);
        /* An absolute value can't exceed the cache size. */
        if (input > conn->cache_size)
            WT_RET_MSG(session, EINVAL, "%s should not exceed cache size", param_name);

        *param = (input * 100.0) / (conn->cache_size);
    }

    return (0);
}

/*
 * __validate_evict_config --
 *     Validate trigger and target values of given configs.
 */
static int
__validate_evict_config(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    WT_EVICT *evict;
    bool shared;

    conn = S2C(session);
    evict = conn->evict;

    WT_RET(__wt_config_gets_none(session, cfg, "shared_cache.name", &cval));
    shared = cval.len != 0;

    /* Debug flags are not yet set when this function runs during connection open. Set it now. */
    WT_RET(__wt_config_gets(session, cfg, "debug_mode.configuration", &cval));
    if (cval.val)
        FLD_SET(conn->debug_flags, WT_CONN_DEBUG_CONFIGURATION);
    else
        FLD_CLR(conn->debug_flags, WT_CONN_DEBUG_CONFIGURATION);

    WT_RET(__wt_config_gets(session, cfg, "eviction_target", &cval));
    evict->eviction_target = (double)cval.val;
    WT_RET(
      __evict_config_abs_to_pct(session, &(evict->eviction_target), "eviction target", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_trigger", &cval));
    evict->eviction_trigger = (double)cval.val;
    WT_RET(
      __evict_config_abs_to_pct(session, &(evict->eviction_trigger), "eviction trigger", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_dirty_target", &cval));
    evict->eviction_dirty_target = (double)cval.val;
    WT_RET(__evict_config_abs_to_pct(
      session, &(evict->eviction_dirty_target), "eviction dirty target", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_dirty_trigger", &cval));
    evict->eviction_dirty_trigger = (double)cval.val;
    WT_RET(__evict_config_abs_to_pct(
      session, &(evict->eviction_dirty_trigger), "eviction dirty trigger", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_updates_target", &cval));
    evict->priv.eviction_updates_target = (double)cval.val;
    WT_RET(__evict_config_abs_to_pct(
      session, &(evict->priv.eviction_updates_target), "eviction updates target", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_updates_trigger", &cval));
    evict->priv.eviction_updates_trigger = (double)cval.val;
    WT_RET(__evict_config_abs_to_pct(
      session, &(evict->priv.eviction_updates_trigger), "eviction updates trigger", shared));

    WT_RET(__wt_config_gets(session, cfg, "eviction_checkpoint_target", &cval));
    evict->eviction_checkpoint_target = (double)cval.val;
    WT_RET(__evict_config_abs_to_pct(
      session, &(evict->eviction_checkpoint_target), "eviction checkpoint target", shared));

    /* Check for invalid configurations and automatically fix them to suitable values. */
    if (evict->eviction_dirty_target > evict->eviction_target) {
        WT_CONFIG_DEBUG(session,
          "config eviction_dirty_target=%f cannot exceed eviction_target=%f. Setting "
          "eviction_dirty_target to %f.",
          evict->eviction_dirty_target, evict->eviction_target, evict->eviction_target);
        evict->eviction_dirty_target = evict->eviction_target;
    }

    if (evict->eviction_checkpoint_target > 0 &&
      evict->eviction_checkpoint_target < evict->eviction_dirty_target) {
        WT_CONFIG_DEBUG(session,
          "config eviction_checkpoint_target=%f cannot be less than eviction_dirty_target=%f. "
          "Setting "
          "eviction_checkpoint_target to %f.",
          evict->eviction_checkpoint_target, evict->eviction_dirty_target,
          evict->eviction_dirty_target);
        evict->eviction_checkpoint_target = evict->eviction_dirty_target;
    }

    if (evict->eviction_dirty_trigger > evict->eviction_trigger) {
        WT_CONFIG_DEBUG(session,
          "config eviction_dirty_trigger=%f cannot exceed eviction_trigger=%f. Setting "
          "eviction_dirty_trigger to %f.",
          evict->eviction_dirty_trigger, evict->eviction_trigger, evict->eviction_trigger);
        evict->eviction_dirty_trigger = evict->eviction_trigger;
    }

    if (evict->priv.eviction_updates_target < DBL_EPSILON) {
        WT_CONFIG_DEBUG(session,
          "config eviction_updates_target (%f) cannot be zero. Setting "
          "to 50%% of eviction_updates_target (%f).",
          evict->priv.eviction_updates_target, evict->eviction_dirty_target / 2);
        evict->priv.eviction_updates_target = evict->eviction_dirty_target / 2;
    }

    if (evict->priv.eviction_updates_trigger < DBL_EPSILON) {
        WT_CONFIG_DEBUG(session,
          "config eviction_updates_trigger (%f) cannot be zero. Setting "
          "to 50%% of eviction_updates_trigger (%f).",
          evict->priv.eviction_updates_trigger, evict->eviction_dirty_trigger / 2);
        evict->priv.eviction_updates_trigger = evict->eviction_dirty_trigger / 2;
    }

    /* Don't allow the trigger to be larger than the overall trigger. */
    if (evict->priv.eviction_updates_trigger > evict->eviction_trigger) {
        WT_CONFIG_DEBUG(session,
          "config eviction_updates_trigger=%f cannot exceed eviction_trigger=%f. Setting "
          "eviction_updates_trigger to %f.",
          evict->priv.eviction_updates_trigger, evict->eviction_trigger, evict->eviction_trigger);
        evict->priv.eviction_updates_trigger = evict->eviction_trigger;
    }

    /* The target size must be lower than the trigger size or we will never get any work done. */
    if (evict->eviction_target >= evict->eviction_trigger)
        WT_RET_MSG(session, EINVAL, "eviction target must be lower than the eviction trigger");
    if (evict->eviction_dirty_target >= evict->eviction_dirty_trigger)
        WT_RET_MSG(
          session, EINVAL, "eviction dirty target must be lower than the eviction dirty trigger");
    if (evict->priv.eviction_updates_target >= evict->priv.eviction_updates_trigger)
        WT_RET_MSG(session, EINVAL,
          "eviction updates target must be lower than the eviction updates trigger");

    return (0);
}

/*
 * __evict_config_local --
 *     Configure eviction.
 */
static int
__evict_config_local(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    WT_EVICT *evict;
    uint32_t evict_threads_max, evict_threads_min;

    conn = S2C(session);
    evict = conn->evict;

    WT_RET(__validate_evict_config(session, cfg));

    WT_RET(__wt_config_gets(session, cfg, "eviction.threads_max", &cval));
    WT_ASSERT(session, cval.val > 0);
    evict_threads_max = (uint32_t)cval.val;

    WT_RET(__wt_config_gets(session, cfg, "eviction.threads_min", &cval));
    WT_ASSERT(session, cval.val > 0);
    evict_threads_min = (uint32_t)cval.val;

    if (evict_threads_min > evict_threads_max)
        WT_RET_MSG(
          session, EINVAL, "eviction=(threads_min) cannot be greater than eviction=(threads_max)");
    conn->evict_threads_max = evict_threads_max;
    conn->evict_threads_min = evict_threads_min;

    WT_RET(__wt_config_gets(session, cfg, "eviction.evict_sample_inmem", &cval));
    conn->evict_sample_inmem = cval.val != 0;

    /* Retrieve the wait time and convert from milliseconds */
    WT_RET(__wt_config_gets(session, cfg, "cache_max_wait_ms", &cval));
    evict->priv.cache_max_wait_us = (uint64_t)(cval.val * WT_THOUSAND);

    /* Retrieve the timeout value and convert from seconds */
    WT_RET(__wt_config_gets(session, cfg, "cache_stuck_timeout_ms", &cval));
    evict->priv.cache_stuck_timeout_ms = (uint64_t)cval.val;

    return (0);
}

/*
 * __wt_eviction_config --
 *     Configure or reconfigure eviction.
 */
int
__wt_eviction_config(WT_SESSION_IMPL *session, const char *cfg[], bool reconfig)
{
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);

    WT_ASSERT(session, conn->evict != NULL);

    WT_RET(__evict_config_local(session, cfg));

    /*
     * Resize the thread group if reconfiguring, otherwise the thread group will be initialized as
     * part of creating the cache.
     */
    if (reconfig)
        WT_RET(__wt_thread_group_resize(session, &conn->evict_threads, conn->evict_threads_min,
          conn->evict_threads_max, WT_THREAD_CAN_WAIT | WT_THREAD_PANIC_FAIL));

    return (0);
}

/*
 * __wt_eviction_stats_update --
 *     Update the eviction statistics for return to the application.
 */
void
__wt_eviction_stats_update(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONNECTION_STATS **stats;
    WT_EVICT *evict;

    conn = S2C(session);
    evict = conn->evict;
    stats = conn->stats;

    WT_STATP_CONN_SET(session, stats, cache_eviction_maximum_page_size,
      __wt_atomic_load64(&evict->evict_max_page_size));
    WT_STATP_CONN_SET(session, stats, cache_eviction_maximum_milliseconds,
      __wt_atomic_load64(&evict->evict_max_ms));
    WT_STATP_CONN_SET(
      session, stats, cache_reentry_hs_eviction_milliseconds, evict->reentry_hs_eviction_ms);

    WT_STATP_CONN_SET(session, stats, cache_eviction_state, __wt_atomic_load32(&evict->flags));
    WT_STATP_CONN_SET(
      session, stats, cache_eviction_aggressive_set, evict->priv.evict_aggressive_score);
    WT_STATP_CONN_SET(session, stats, cache_eviction_empty_score, evict->priv.evict_empty_score);

    WT_STATP_CONN_SET(session, stats, cache_eviction_active_workers,
      __wt_atomic_load32(&conn->evict_threads.current_threads));
    WT_STATP_CONN_SET(
      session, stats, cache_eviction_stable_state_workers, evict->priv.evict_tune_workers_best);

    /*
     * The number of files with active walks ~= number of hazard pointers in the walk session. Note:
     * reading without locking.
     */
    if (__wt_atomic_loadbool(&conn->evict_server_running))
        WT_STATP_CONN_SET(session, stats, cache_eviction_walks_active,
          evict->priv.walk_session->hazards.num_active);
}

/*
 * __wt_eviction_create --
 *     Initialize Eviction.
 */
int
__wt_eviction_create(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_EVICT *evict;
    int i;

    conn = S2C(session);

    WT_ASSERT(session, conn->evict == NULL);

    WT_RET(__wt_calloc_one(session, &conn->evict));

    evict = conn->evict;

    /* Use a common routine for run-time configuration options. */
    WT_RET(__wt_eviction_config(session, cfg, false));

    /*
     * The lowest possible page read-generation has a special meaning, it marks a page for forcible
     * eviction; don't let it happen by accident.
     */
    evict->priv.read_gen_oldest = WT_READGEN_START_VALUE;
    __wt_atomic_store64(&evict->priv.read_gen, WT_READGEN_START_VALUE);

    WT_RET(__wt_cond_auto_alloc(
      session, "cache eviction server", 10 * WT_THOUSAND, WT_MILLION, &evict->priv.evict_cond));
    WT_RET(__wt_spin_init(session, &evict->priv.evict_pass_lock, "evict pass"));
    WT_RET(__wt_spin_init(session, &evict->priv.evict_queue_lock, "cache eviction queue"));
    WT_RET(__wt_spin_init(session, &evict->priv.evict_walk_lock, "cache walk"));
    if ((ret = __wt_open_internal_session(conn, "evict pass", false, WT_SESSION_NO_DATA_HANDLES, 0,
           &evict->priv.walk_session)) != 0)
        WT_RET_MSG(NULL, ret, "Failed to create session for eviction walks");

    /* Allocate the LRU eviction queue. */
    evict->priv.evict_slots = WT_EVICT_WALK_BASE + WT_EVICT_WALK_INCR;
    for (i = 0; i < WT_EVICT_QUEUE_MAX; ++i) {
        WT_RET(__wt_calloc_def(
          session, evict->priv.evict_slots, &evict->priv.evict_queues[i].evict_queue));
        WT_RET(__wt_spin_init(session, &evict->priv.evict_queues[i].evict_lock, "cache eviction"));
    }

    /* Ensure there are always non-NULL queues. */
    evict->priv.evict_current_queue = evict->priv.evict_fill_queue = &evict->priv.evict_queues[0];
    evict->priv.evict_other_queue = &evict->priv.evict_queues[1];
    evict->priv.evict_urgent_queue = &evict->priv.evict_queues[WT_EVICT_URGENT_QUEUE];

    /*
     * We get/set some values in the cache statistics (rather than have two copies), configure them.
     */
    __wt_eviction_stats_update(session);
    return (0);
}

/*
 * __wt_eviction_destroy --
 *     Destroy Eviction.
 */
int
__wt_eviction_destroy(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_EVICT *evict;
    int i;

    conn = S2C(session);
    evict = conn->evict;

    if (evict == NULL)
        return (0);

    __wt_cond_destroy(session, &evict->priv.evict_cond);
    __wt_spin_destroy(session, &evict->priv.evict_pass_lock);
    __wt_spin_destroy(session, &evict->priv.evict_queue_lock);
    __wt_spin_destroy(session, &evict->priv.evict_walk_lock);
    if (evict->priv.walk_session != NULL)
        WT_TRET(__wt_session_close_internal(evict->priv.walk_session));

    for (i = 0; i < WT_EVICT_QUEUE_MAX; ++i) {
        __wt_spin_destroy(session, &evict->priv.evict_queues[i].evict_lock);
        __wt_free(session, evict->priv.evict_queues[i].evict_queue);
    }

    __wt_free(session, conn->evict);
    return (ret);
}
