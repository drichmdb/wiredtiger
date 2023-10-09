/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __chunkcache_create_metadata_file --
 *     Create the table that will persistently track what chunk cache content is on disk.
 */
static int
__chunkcache_create_metadata_file(
  WT_SESSION_IMPL *session, uint64_t capacity, unsigned int hashtable_size, size_t chunk_size)
{
    char cfg[128];
    WT_RET(__wt_snprintf(cfg, sizeof(cfg), WT_CC_APP_META_FORMAT "," WT_CC_META_CONFIG, capacity,
      hashtable_size, chunk_size));

    return (__wt_session_create(session, WT_CC_METAFILE_URI, cfg));
}

/*
 * __chunkcache_get_metadata_config --
 *     If present, retrieve the on-disk configuration for the chunk cache metadata file. The caller
 *     must only use *config if *found is true. The caller is responsible for freeing the memory
 *     allocated into *config.
 */
static int
__chunkcache_get_metadata_config(WT_SESSION_IMPL *session, char **config)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    char *tmp;

    *config = NULL;

    WT_RET(__wt_metadata_cursor(session, &cursor));
    cursor->set_key(cursor, WT_CC_METAFILE_URI);
    WT_ERR(cursor->search(cursor));

    WT_ERR(cursor->get_value(cursor, &tmp));
    WT_ERR(__wt_strdup(session, tmp, config));

err:
    WT_TRET(__wt_metadata_cursor_release(session, &cursor));
    return (ret);
}

/*
 * __chunkcache_verify_metadata_config --
 *     Check that the existing chunk cache configuration is compatible with our current
 *     configuration (and ergo, whether we can reuse the chunk cache contents).
 */
static int
__chunkcache_verify_metadata_config(WT_SESSION_IMPL *session, char *md_config, uint64_t capacity,
  unsigned int hashtable_size, size_t chunk_size)
{
    WT_DECL_RET;
    char tmp[128];

    WT_RET(
      __wt_snprintf(tmp, sizeof(tmp), WT_CC_APP_META_FORMAT, capacity, hashtable_size, chunk_size));

    if (strstr(md_config, tmp) == NULL) {
        __wt_verbose_error(session, WT_VERB_CHUNKCACHE,
          "stored chunk cache config (%s) incompatible with runtime config (%s)", md_config, tmp);
        return (-1);
    }

    return (ret);
}

/*
 * __chunkcache_metadata_run_chk --
 *     Check to decide if the chunk cache metadata server should continue running.
 */
static bool
__chunkcache_metadata_run_chk(WT_SESSION_IMPL *session)
{
    return (FLD_ISSET(S2C(session)->server_flags, WT_CONN_SERVER_CHUNKCACHE_METADATA));
}

/*
 * __chunkcache_metadata_insert --
 *     Insert a specific work queue entry into the chunk cache metadata file.
 */
static int
__chunkcache_metadata_insert(WT_CURSOR *cursor, WT_CHUNKCACHE_METADATA_WORK_UNIT *entry)
{
    cursor->set_key(cursor, entry->name, entry->id, entry->file_offset);
    cursor->set_value(cursor, entry->cache_offset, entry->data_sz);

    return (cursor->insert(cursor));
}

/*
 * __chunkcache_metadata_delete --
 *     Remove a specific work queue entry into the chunk cache metadata file.
 */
static int
__chunkcache_metadata_delete(WT_CURSOR *cursor, WT_CHUNKCACHE_METADATA_WORK_UNIT *entry)
{
    cursor->set_key(cursor, entry->name, entry->id, entry->file_offset);
    cursor->set_value(cursor, entry->cache_offset, entry->data_sz);

    return (cursor->remove(cursor));
}

/*
 * __chunkcache_metadata_pop_work --
 *     Pop a work unit from the queue. The caller is responsible for freeing the returned work unit
 *     structure.
 */
static void
__chunkcache_metadata_pop_work(WT_SESSION_IMPL *session, WT_CHUNKCACHE_METADATA_WORK_UNIT **entryp)
{
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);

    __wt_spin_lock(session, &conn->chunkcache_metadata_lock);
    if ((*entryp = TAILQ_FIRST(&conn->chunkcache_metadataqh)) != NULL)
        TAILQ_REMOVE(&conn->chunkcache_metadataqh, *entryp, q);
    __wt_spin_unlock(session, &conn->chunkcache_metadata_lock);

    WT_STAT_CONN_INCR(session, chunkcache_metadata_work_units_dequeued);
}

/*
 * __chunkcache_metadata_work --
 *     Pop chunk cache work items off the queue, and write out the metadata.
 */
static int
__chunkcache_metadata_work(WT_SESSION_IMPL *session)
{
    WT_CHUNKCACHE_METADATA_WORK_UNIT *entry;
    WT_CURSOR *cursor;
    WT_DECL_RET;

    WT_RET(session->iface.open_cursor(
      &session->iface, WT_CC_METAFILE_URI, NULL, NULL, &cursor));

    entry = NULL;
    for (int i = 0; i < WT_CHUNKCACHE_METADATA_MAX_WORK; i++) {
        if (!__chunkcache_metadata_run_chk(session))
            break;

        __chunkcache_metadata_pop_work(session, &entry);
        if (entry == NULL)
            break;

        /* TODO where to check TAILQ_EMPTY? */
        if (entry->type == WT_CHUNKCACHE_METADATA_WORK_INS)
            WT_ERR(__chunkcache_metadata_insert(cursor, entry));
        else if (entry->type == WT_CHUNKCACHE_METADATA_WORK_DEL)
            WT_ERR_NOTFOUND_OK(__chunkcache_metadata_delete(cursor, entry), false);
        else {
            __wt_verbose_error(
              session, WT_VERB_CHUNKCACHE, "got messed up event type %d\n", entry->type);
            ret = -1;
            goto err;
        }

        __wt_free(session, entry);
        entry = NULL;
    }

err:
    if (cursor != NULL)
        WT_TRET(cursor->close(cursor));
    if (entry != NULL)
        __wt_free(session, entry);
    return (ret);
}

/*
 * __chunkcache_metadata_server --
 *     Dispatch chunks of work (or stop the server) whenever we're signalled to do so.
 */
static WT_THREAD_RET
__chunkcache_metadata_server(void *arg)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_SESSION_IMPL *session;
    uint64_t cond_time_us;
    bool signalled;

    session = arg;
    conn = S2C(session);
    cond_time_us = 1 * WT_MILLION;

    for (;;) {
        /* Wait until the next event. */
        __wt_cond_wait_signal(session, conn->chunkcache_metadata_cond, cond_time_us,
          __chunkcache_metadata_run_chk, &signalled);

        if (!__chunkcache_metadata_run_chk(session))
            break;

        if (signalled)
            WT_ERR(__chunkcache_metadata_work(session));
    }

    if (0) {
err:
        WT_IGNORE_RET(__wt_panic(session, ret, "%s", "chunk cache metadata server error"));
    }

    return (WT_THREAD_RET_VALUE);
}

/*
 * __wt_chunkcache_metadata_create --
 *     Start the server component of the chunk cache metadata subsystem.
 */
int
__wt_chunkcache_metadata_create(WT_SESSION_IMPL *session)
{
    WT_CHUNKCACHE *chunkcache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    char *metadata_config;

    conn = S2C(session);
    chunkcache = &conn->chunkcache;

    /* Retrieve the chunk cache metadata config, and ensure it matches our startup config. */
    ret = __chunkcache_get_metadata_config(session, &metadata_config);
    if (ret == WT_NOTFOUND) {
        WT_RET(__chunkcache_create_metadata_file(
          session, chunkcache->capacity, chunkcache->hashtable_size, chunkcache->chunk_size));
        __wt_verbose(session, WT_VERB_CHUNKCACHE, "%s", "created chunkcache metadata file");
    } else if (ret == 0) {
        WT_RET(__chunkcache_verify_metadata_config(session, metadata_config, chunkcache->capacity,
          chunkcache->hashtable_size, chunkcache->chunk_size));
        __wt_verbose(session, WT_VERB_CHUNKCACHE, "%s", "reused chunkcache metadata file");
    } else {
        WT_RET(ret);
    }
    __wt_free(session, metadata_config);

    /* Start the internal thread. */
    WT_ERR(__wt_cond_alloc(session, "chunkcache metadata", &conn->chunkcache_metadata_cond));
    FLD_SET(conn->server_flags, WT_CONN_SERVER_CHUNKCACHE_METADATA);

    /* TODO set isolation */
    WT_ERR(__wt_open_internal_session(
      conn, "chunkcache-metadata-server", true, 0, 0, &conn->chunkcache_metadata_session));
    session = conn->chunkcache_metadata_session;

    /* Start the thread. */
    WT_ERR(__wt_thread_create(
      session, &conn->chunkcache_metadata_tid, __chunkcache_metadata_server, session));
    conn->chunkcache_metadata_tid_set = true;

    if (0) {
err:
        FLD_CLR(conn->server_flags, WT_CONN_SERVER_CHUNKCACHE_METADATA);
        WT_TRET(__wt_chunkcache_metadata_destroy(session));
    }

    return (ret);
}

/*
 * __wt_chunkcache_metadata_destroy --
 *     Destroy the chunk cache metadata server thread.
 */
int
__wt_chunkcache_metadata_destroy(WT_SESSION_IMPL *session)
{
    WT_CHUNKCACHE_METADATA_WORK_UNIT *entry;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;

    conn = S2C(session);

    FLD_CLR(conn->server_flags, WT_CONN_SERVER_CHUNKCACHE_METADATA);
    if (conn->chunkcache_metadata_tid_set) {
        WT_ASSERT(session, conn->chunkcache_metadata_cond != NULL);
        WT_TRET(__wt_thread_join(session, &conn->chunkcache_metadata_tid));
        conn->chunkcache_metadata_tid_set = false;
        while ((entry = TAILQ_FIRST(&conn->chunkcache_metadataqh)) != NULL) {
            TAILQ_REMOVE(&conn->chunkcache_metadataqh, entry, q);
            __wt_free(session, entry);
        }
    }

    if (conn->chunkcache_metadata_session != NULL) {
        WT_TRET(__wt_session_close_internal(conn->chunkcache_metadata_session));
        conn->chunkcache_metadata_session = NULL;
    }

    __wt_cond_destroy(session, &conn->chunkcache_metadata_cond);

    return (ret);
}
