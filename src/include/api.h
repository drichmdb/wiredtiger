/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#ifdef HAVE_DIAGNOSTIC
/*
 * Capture cases where a single session handle is used by multiple threads in parallel. The check
 * isn't trivial because some API calls re-enter via public API entry points and the session with ID
 * 0 is the default session in the connection handle which can be used across multiple threads.
 */
#define WT_SINGLE_THREAD_CHECK_START(s)                                           \
    {                                                                             \
        uintmax_t __tmp_api_tid;                                                  \
        __wt_thread_id(&__tmp_api_tid);                                           \
                                                                                  \
        /*                                                                        \
         * Only a single thread should use this session at a time. It's ok        \
         * (but unexpected) if different threads use the session consecutively,   \
         * but concurrent access is not allowed. Verify this by having the thread \
         * take a lock on first API access. Failing to take the lock implies      \
         * another thread holds it and we're attempting concurrent access of the  \
         * session.                                                               \
         *                                                                        \
         * The default session (ID == 0) is an exception where concurrent access  \
         * is allowed. We can also skip taking the lock if we're re-entrant and   \
         * already hold it.                                                       \
         */                                                                       \
        if ((s)->id != 0 && (s)->thread_check.owning_thread != __tmp_api_tid) {   \
            WT_ASSERT((s), __wt_spin_trylock((s), &(s)->thread_check.lock) == 0); \
            (s)->thread_check.owning_thread = __tmp_api_tid;                      \
        }                                                                         \
                                                                                  \
        ++(s)->thread_check.entry_count;                                          \
    }

#define WT_SINGLE_THREAD_CHECK_STOP(s)                          \
    {                                                           \
        uintmax_t __tmp_api_tid;                                \
        __wt_thread_id(&__tmp_api_tid);                         \
        if (--((s)->thread_check.entry_count) == 0) {           \
            if ((s)->id != 0) {                                 \
                (s)->thread_check.owning_thread = 0;            \
                __wt_spin_unlock((s), &(s)->thread_check.lock); \
            }                                                   \
        }                                                       \
    }
#else
#define WT_SINGLE_THREAD_CHECK_START(s)
#define WT_SINGLE_THREAD_CHECK_STOP(s)
#endif

#define API_SESSION_PUSH(s, struct_name, func_name, dh)                                      \
    WT_DATA_HANDLE *__olddh = (s)->dhandle;                                                  \
    const char *__oldname;                                                                   \
    /* If this isn't an API reentry, the name should be NULL and the counter should be 0. */ \
    WT_ASSERT(s, (s)->name != NULL || (s)->api_call_counter == 0);                           \
    __oldname = (s)->name;                                                                   \
    ++(s)->api_call_counter;                                                                 \
    if ((s)->api_call_counter == 1)                                                          \
        (void)__wt_atomic_add32(&S2C(s)->active_api_count, 1);                               \
    (s)->dhandle = (dh);                                                                     \
    (s)->name = (s)->lastop = #struct_name "." #func_name

#define API_SESSION_POP(s)          \
    (s)->dhandle = __olddh;         \
    (s)->name = __oldname;          \
    --(s)->api_call_counter;        \
    if ((s)->api_call_counter == 0) \
        (void)__wt_atomic_sub32(&S2C(s)->active_api_count, 1);

/* Standard entry points to the API: declares/initializes local variables. */
#define API_SESSION_INIT(s, struct_name, func_name, dh)                 \
    WT_TRACK_OP_DECL;                                                   \
    API_SESSION_PUSH(s, struct_name, func_name, dh);                    \
    /*                                                                  \
     * No code before this line, otherwise error handling won't be      \
     * correct.                                                         \
     */                                                                 \
    WT_ERR(WT_SESSION_CHECK_PANIC(s));                                  \
    WT_SINGLE_THREAD_CHECK_START(s);                                    \
    WT_TRACK_OP_INIT(s);                                                \
    if ((s)->api_call_counter == 1 && !F_ISSET(s, WT_SESSION_INTERNAL)) \
        __wt_op_timer_start(s);                                         \
    /* Reset wait time if this isn't an API reentry. */                 \
    if ((s)->api_call_counter == 1)                                     \
        (s)->cache_wait_us = 0;                                         \
    __wt_verbose((s), WT_VERB_API, "%s", "CALL: " #struct_name ":" #func_name)

#define API_CALL_NOCONF(s, struct_name, func_name, dh) \
    do {                                               \
        bool __set_err = true;                         \
    API_SESSION_INIT(s, struct_name, func_name, dh)

#define API_CALL(s, struct_name, func_name, dh, config, cfg)                                \
    do {                                                                                    \
        bool __set_err = true;                                                              \
        const char *(cfg)[] = {WT_CONFIG_BASE(s, struct_name##_##func_name), config, NULL}; \
        API_SESSION_INIT(s, struct_name, func_name, dh);                                    \
        if ((config) != NULL)                                                               \
    WT_ERR(__wt_config_check((s), WT_CONFIG_REF(s, struct_name##_##func_name), (config), 0))

#define API_END(s, ret)                                                                    \
    if ((s) != NULL) {                                                                     \
        WT_TRACK_OP_END(s);                                                                \
        WT_SINGLE_THREAD_CHECK_STOP(s);                                                    \
        if ((ret) != 0 && __set_err)                                                       \
            __wt_txn_err_set(s, (ret));                                                    \
        if ((s)->api_call_counter == 1 && !F_ISSET(s, WT_SESSION_INTERNAL))                \
            __wt_op_timer_stop(s);                                                         \
        /*                                                                                 \
         * We should not leave any history store cursor open when return from an api call. \
         * However, we cannot do a stricter check before WT-7247 is resolved.              \
         */                                                                                \
        WT_ASSERT(s, (s)->api_call_counter > 1 || (s)->hs_cursor_counter <= 3);            \
        /*                                                                                 \
         * No code after this line, otherwise error handling                               \
         * won't be correct.                                                               \
         */                                                                                \
        API_SESSION_POP(s);                                                                \
    }                                                                                      \
    }                                                                                      \
    while (0)

/* An API call wrapped in a transaction if necessary. */
#define TXN_API_CALL(s, struct_name, func_name, dh, config, cfg)            \
    do {                                                                    \
        bool __autotxn = false, __update = false;                           \
        API_CALL(s, struct_name, func_name, dh, config, cfg);               \
        __autotxn = !F_ISSET((s)->txn, WT_TXN_AUTOCOMMIT | WT_TXN_RUNNING); \
        if (__autotxn)                                                      \
            F_SET((s)->txn, WT_TXN_AUTOCOMMIT);                             \
        __update = !F_ISSET((s)->txn, WT_TXN_UPDATE);                       \
        if (__update)                                                       \
            F_SET((s)->txn, WT_TXN_UPDATE);

/* An API call wrapped in a transaction if necessary. */
#define TXN_API_CALL_NOCONF(s, struct_name, func_name, dh)                  \
    do {                                                                    \
        bool __autotxn = false, __update = false;                           \
        API_CALL_NOCONF(s, struct_name, func_name, dh);                     \
        __autotxn = !F_ISSET((s)->txn, WT_TXN_AUTOCOMMIT | WT_TXN_RUNNING); \
        if (__autotxn)                                                      \
            F_SET((s)->txn, WT_TXN_AUTOCOMMIT);                             \
        __update = !F_ISSET((s)->txn, WT_TXN_UPDATE);                       \
        if (__update)                                                       \
            F_SET((s)->txn, WT_TXN_UPDATE);

/* End a transactional API call, optional retry on rollback. */
#define TXN_API_END(s, ret, retry)                                  \
    API_END(s, ret);                                                \
    if (__update)                                                   \
        F_CLR((s)->txn, WT_TXN_UPDATE);                             \
    if (__autotxn) {                                                \
        if (F_ISSET((s)->txn, WT_TXN_AUTOCOMMIT)) {                 \
            F_CLR((s)->txn, WT_TXN_AUTOCOMMIT);                     \
            if ((retry) && (ret) == WT_ROLLBACK) {                  \
                (ret) = 0;                                          \
                WT_STAT_CONN_DATA_INCR(s, autocommit_update_retry); \
                continue;                                           \
            }                                                       \
        } else if ((ret) == 0)                                      \
            (ret) = __wt_txn_commit((s), NULL);                     \
        else {                                                      \
            if (retry)                                              \
                WT_TRET(__wt_session_copy_values(s));               \
            WT_TRET(__wt_txn_rollback((s), NULL));                  \
            if ((retry) && (ret) == WT_ROLLBACK) {                  \
                (ret) = 0;                                          \
                WT_STAT_CONN_DATA_INCR(s, autocommit_update_retry); \
                continue;                                           \
            }                                                       \
            WT_TRET(__wt_session_reset_cursors(s, false));          \
        }                                                           \
    }                                                               \
    break;                                                          \
    }                                                               \
    /* !!!! This is a while(1) loop. !!!! */                        \
    while (1)

/*
 * In almost all cases, API_END is returning immediately, make it simple. If a session or connection
 * method is about to return WT_NOTFOUND (some underlying object was not found), map it to ENOENT,
 * only cursor methods return WT_NOTFOUND.
 */
#define API_END_RET(s, ret) \
    API_END(s, ret);        \
    return (ret)

#define API_END_STAT(s, ret, api)                   \
    do {                                            \
        if ((ret) != 0 && ((ret) != WT_NOTFOUND)) { \
            WT_STAT_CONN_DATA_INCR(s, api##_error); \
        }                                           \
    } while (0)

#define API_RET_STAT(s, ret, api)  \
    do {                           \
        API_END_STAT(s, ret, api); \
        return ((ret));            \
    } while (0)

#define API_END_RET_STAT(s, ret, api) \
    do {                              \
        API_END_STAT(s, ret, api);    \
        API_END_RET(s, ret);          \
    } while (0)

#define API_END_RET_NOTFOUND_MAP(s, ret) \
    API_END(s, ret);                     \
    return ((ret) == WT_NOTFOUND ? ENOENT : (ret))

/*
 * Used in cases where transaction error should not be set, but the error is returned from the API.
 * Success is passed to the API_END macro. If the method is about to return WT_NOTFOUND map it to
 * ENOENT.
 */
#define API_END_RET_NO_TXN_ERROR(s, ret) \
    API_END(s, 0);                       \
    return ((ret) == WT_NOTFOUND ? ENOENT : (ret))

#define API_USER_ENTRY(s) (s)->api_call_counter == 1

#define CONNECTION_API_CALL(conn, s, func_name, config, cfg) \
    s = (conn)->default_session;                             \
    API_CALL(s, WT_CONNECTION, func_name, NULL, config, cfg)

#define CONNECTION_API_CALL_NOCONF(conn, s, func_name) \
    s = (conn)->default_session;                       \
    API_CALL_NOCONF(s, WT_CONNECTION, func_name, NULL)

#define SESSION_API_CALL_PREPARE_ALLOWED(s, func_name, config, cfg) \
    API_CALL(s, WT_SESSION, func_name, NULL, config, cfg)

#define SESSION_API_CALL_PREPARE_ALLOWED_NOCONF(s, func_name) \
    API_CALL_NOCONF(s, WT_SESSION, func_name, NULL)

#define SESSION_API_CALL_PREPARE_NOT_ALLOWED(s, ret, func_name, config, cfg) \
    API_CALL(s, WT_SESSION, func_name, NULL, config, cfg);                   \
    SESSION_API_PREPARE_CHECK(s, ret, WT_SESSION, func_name)

#define SESSION_API_CALL_PREPARE_NOT_ALLOWED_NOCONF(s, ret, func_name) \
    API_CALL_NOCONF(s, WT_SESSION, func_name, NULL);                   \
    SESSION_API_PREPARE_CHECK(s, ret, WT_SESSION, func_name)

#define SESSION_API_PREPARE_CHECK(s, ret, struct_name, func_name)                                \
    do {                                                                                         \
        if ((s)->api_call_counter == 1) {                                                        \
            (ret) = __wt_txn_context_prepare_check(s);                                           \
            if ((ret) != 0) {                                                                    \
                /*                                                                               \
                 * Don't set the error on transaction. We don't want to rollback the transaction \
                 * because of this error.                                                        \
                 */                                                                              \
                __set_err = false;                                                               \
                goto err;                                                                        \
            }                                                                                    \
        }                                                                                        \
    } while (0)

#define SESSION_API_CALL(s, ret, func_name, config, cfg)   \
    API_CALL(s, WT_SESSION, func_name, NULL, config, cfg); \
    SESSION_API_PREPARE_CHECK(s, ret, WT_SESSION, func_name)

#define SESSION_API_CALL_NOCONF(s, func_name) API_CALL_NOCONF(s, WT_SESSION, func_name, NULL)

#define SESSION_TXN_API_CALL(s, ret, func_name, config, cfg)   \
    TXN_API_CALL(s, WT_SESSION, func_name, NULL, config, cfg); \
    SESSION_API_PREPARE_CHECK(s, ret, WT_SESSION, func_name)

#define CURSOR_API_CALL(cur, s, ret, func_name, bt)                                                \
    (s) = CUR2S(cur);                                                                              \
    API_CALL_NOCONF(s, WT_CURSOR, func_name, ((bt) == NULL) ? NULL : ((WT_BTREE *)(bt))->dhandle); \
    SESSION_API_PREPARE_CHECK(s, ret, WT_CURSOR, func_name);                                       \
    if (F_ISSET(cur, WT_CURSTD_CACHED))                                                            \
    WT_ERR(__wt_cursor_cached(cur))

#define CURSOR_API_CALL_CONF(cur, s, ret, func_name, config, cfg, bt)                             \
    (s) = CUR2S(cur);                                                                             \
    API_CALL(                                                                                     \
      s, WT_CURSOR, func_name, ((bt) == NULL) ? NULL : ((WT_BTREE *)(bt))->dhandle, config, cfg); \
    SESSION_API_PREPARE_CHECK(s, ret, WT_CURSOR, func_name);                                      \
    if (F_ISSET(cur, WT_CURSTD_CACHED))                                                           \
    WT_ERR(__wt_cursor_cached(cur))

#define CURSOR_API_CALL_PREPARE_ALLOWED(cur, s, func_name, bt)                                     \
    (s) = CUR2S(cur);                                                                              \
    API_CALL_NOCONF(s, WT_CURSOR, func_name, ((bt) == NULL) ? NULL : ((WT_BTREE *)(bt))->dhandle); \
    if (F_ISSET(cur, WT_CURSTD_CACHED))                                                            \
    WT_ERR(__wt_cursor_cached(cur))

/*
 * API_RETRYABLE and API_RETRYABLE_END are used to wrap API calls so that they are silently
 * retried on rollback errors. Generally, these only need to be used with readonly APIs, as
 * writable APIs have their own retry code via TXN_API_CALL.  These macros may be used with
 * *API_CALL and API_END* provided they are ordered in a balanced way.
 */
#define API_RETRYABLE(s) do {

#define API_RETRYABLE_END(s, ret)                                                                \
    if ((ret) != WT_ROLLBACK || F_ISSET((s)->txn, WT_TXN_RUNNING) || (s)->api_call_counter != 1) \
        break;                                                                                   \
    (ret) = 0;                                                                                   \
    WT_STAT_CONN_DATA_INCR(s, autocommit_readonly_retry);                                        \
    }                                                                                            \
    /* !!!! This is a while(1) loop. !!!! */                                                     \
    while (1)

#define JOINABLE_CURSOR_CALL_CHECK(cur) \
    if (F_ISSET(cur, WT_CURSTD_JOINED)) \
    WT_ERR(__wt_curjoin_joined(cur))

#define JOINABLE_CURSOR_API_CALL(cur, s, ret, func_name, bt) \
    CURSOR_API_CALL(cur, s, ret, func_name, bt);             \
    JOINABLE_CURSOR_CALL_CHECK(cur)

#define JOINABLE_CURSOR_API_CALL_CONF(cur, s, ret, func_name, config, cfg, bt) \
    CURSOR_API_CALL_CONF(cur, s, ret, func_name, config, cfg, bt);             \
    JOINABLE_CURSOR_CALL_CHECK(cur)

#define JOINABLE_CURSOR_API_CALL_PREPARE_ALLOWED(cur, s, func_name, bt) \
    CURSOR_API_CALL_PREPARE_ALLOWED(cur, s, func_name, bt);             \
    JOINABLE_CURSOR_CALL_CHECK(cur)

#define CURSOR_REMOVE_API_CALL(cur, s, ret, bt)                                   \
    (s) = CUR2S(cur);                                                             \
    TXN_API_CALL_NOCONF(                                                          \
      s, WT_CURSOR, remove, ((bt) == NULL) ? NULL : ((WT_BTREE *)(bt))->dhandle); \
    SESSION_API_PREPARE_CHECK(s, ret, WT_CURSOR, remove)

#define JOINABLE_CURSOR_REMOVE_API_CALL(cur, s, ret, bt) \
    CURSOR_REMOVE_API_CALL(cur, s, ret, bt);             \
    JOINABLE_CURSOR_CALL_CHECK(cur)

#define CURSOR_UPDATE_API_CALL_BTREE(cur, s, ret, func_name)                                  \
    (s) = CUR2S(cur);                                                                         \
    TXN_API_CALL_NOCONF(s, WT_CURSOR, func_name, ((WT_CURSOR_BTREE *)(cur))->dhandle);        \
    SESSION_API_PREPARE_CHECK(s, ret, WT_CURSOR, func_name);                                  \
    if (F_ISSET(S2C(s), WT_CONN_IN_MEMORY) && !F_ISSET(CUR2BT(cur), WT_BTREE_IGNORE_CACHE) && \
      __wt_cache_full(s))                                                                     \
        WT_ERR(WT_CACHE_FULL);

#define CURSOR_UPDATE_API_CALL(cur, s, ret, func_name)  \
    (s) = CUR2S(cur);                                   \
    TXN_API_CALL_NOCONF(s, WT_CURSOR, func_name, NULL); \
    SESSION_API_PREPARE_CHECK(s, ret, WT_CURSOR, func_name)

#define JOINABLE_CURSOR_UPDATE_API_CALL(cur, s, ret, func_name) \
    CURSOR_UPDATE_API_CALL(cur, s, ret, func_name);             \
    JOINABLE_CURSOR_CALL_CHECK(cur)

#define CURSOR_UPDATE_API_END_RETRY(s, ret, retry) \
    if ((ret) == WT_PREPARE_CONFLICT)              \
        (ret) = WT_ROLLBACK;                       \
    TXN_API_END(s, ret, retry)

#define CURSOR_UPDATE_API_END(s, ret) CURSOR_UPDATE_API_END_RETRY(s, ret, true)

#define CURSOR_UPDATE_API_END_RETRY_STAT(s, ret, retry, api) \
    if ((ret) == WT_PREPARE_CONFLICT)                        \
        (ret) = WT_ROLLBACK;                                 \
    API_END_STAT(s, ret, api);                               \
    TXN_API_END(s, ret, retry)

#define CURSOR_UPDATE_API_END_STAT(s, ret, api) CURSOR_UPDATE_API_END_RETRY_STAT(s, ret, true, api)

/*
 * Calling certain top level APIs allows for internal repositioning of cursors to facilitate
 * eviction of hot pages. These macros facilitate tracking when that is OK.
 */
#define CURSOR_REPOSITION_ENTER(c, s)                                      \
    if (FLD_ISSET(S2C(s)->debug_flags, WT_CONN_DEBUG_CURSOR_REPOSITION) && \
      (s)->api_call_counter == 1)                                          \
    F_SET((c), WT_CURSTD_EVICT_REPOSITION)

#define CURSOR_REPOSITION_END(c, s)                                        \
    if (FLD_ISSET(S2C(s)->debug_flags, WT_CONN_DEBUG_CURSOR_REPOSITION) && \
      (s)->api_call_counter == 1)                                          \
    F_CLR((c), WT_CURSTD_EVICT_REPOSITION)

/*
 * Track cursor API calls, so we can know how many are in the library at a point in time. These need
 * to be balanced. If the api call counter is zero, it means these have been used in the wrong order
 * compared to the other enter/end macros.
 */
#define CURSOR_API_TRACK_START(s)               \
    WT_ASSERT((s), (s)->api_call_counter != 0); \
    if ((s)->api_call_counter == 1)             \
        (void)__wt_atomic_add32(&S2C(s)->active_api_cursor_count, 1);

#define CURSOR_API_TRACK_END(s)                 \
    WT_ASSERT((s), (s)->api_call_counter != 0); \
    if ((s)->api_call_counter == 1)             \
        (void)__wt_atomic_sub32(&S2C(s)->active_api_cursor_count, 1);

/*
 * Macros to set up APIs that use compiled configuration strings.
 */
#define WT_DECL_CONF(h, n, conf)  \
    WT_CONF_API_TYPE(h, n) _conf; \
    WT_CONF *conf = NULL

#define API_CONF(session, h, n, cfg, conf)                                      \
    WT_ERR(__wt_conf_compile_api_call(session, WT_CONFIG_REF(session, h##_##n), \
      WT_CONFIG_ENTRY_##h##_##n, cfg[1], &_conf, sizeof(_conf), &conf))

#define SESSION_API_CONF(session, n, cfg, conf) API_CONF(session, WT_SESSION, n, cfg, conf)

/*
 * There is currently nothing to free, so this is a placeholder for any other cleanup we need in the
 * future.
 */
#define API_CONF_END(session, conf)
