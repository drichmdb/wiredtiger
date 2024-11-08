/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <wiredtiger.h>
#include <wiredtiger_ext.h>

/*
 * In theory, extensions should not call into WT functions willy-nilly, but the swap functions are
 * inlined. We could call the system's swap functions directly, and/or write our own, but we'd
 * duplicate some existing logic.
 *
 * Possibly some functions like swap should live in a more general library than WT.
 */
#include <swap.h>

#include "palm_kv.h"
#define MEGABYTE (1024 * 1024)

/*
 * LMDB requires the number of tables to be known at startup. If we add any more tables, we need to
 * increment this.
 */
#define PALM_MAX_DBI 3

/*
 * The PAGE_KEY is the on disk format for the key of the pages table. The value is a set of bytes,
 * representing the raw page bytes or delta bytes.
 */
typedef struct PAGE_KEY {
    uint64_t table_id;
    uint64_t page_id;
    uint64_t checkpoint_id;
    uint64_t revision;
    uint32_t is_delta;

    /*
     * These are not really things we key on, but they are more convenient to store in the key
     * rather than the data.
     */
    uint64_t backlink;
    uint64_t base;
    uint32_t flags;

    /* To simulate materialization delays, this is the timestamp this record becomes available. */
    uint64_t timestamp_materialized_us;
} PAGE_KEY;

static bool need_swap = true; /* TODO: derive this */

/*
 * Byte swap a page key so that it sorts in the expected order.
 */
static void
swap_page_key(const PAGE_KEY *src, PAGE_KEY *dest)
{
    if (!need_swap)
        return;

    if (dest != src)
        /* Copy all values by default. */
        *dest = *src;

    /*
     * We don't need to swap all the fields in the key, only the ones that we use in comparisons.
     * Other fields in the key that we don't swap are more like data fields, but they are more
     * convenient to keep in the key.
     */
    dest->table_id = __wt_bswap64(src->table_id);
    dest->page_id = __wt_bswap64(src->page_id);
    dest->checkpoint_id = __wt_bswap64(src->checkpoint_id);
    dest->revision = __wt_bswap64(src->revision);
    dest->is_delta = __wt_bswap32(src->is_delta);
}

/*
 * True if and only if the result matches the table and page and is materialized.
 */
#define RESULT_MATCH(result_key, _table_id, _page_id, _now)                          \
    ((result_key)->table_id == (_table_id) && (result_key)->page_id == (_page_id) && \
      _now > (result_key)->timestamp_materialized_us)

#ifdef PALM_KV_DEBUG
/* Show the contents of the PAGE_KEY to stderr.  This can be useful for debugging. */
#define SHOW_PAGE_KEY(pk, label)                                                                   \
    fprintf(stderr, "  %s:  t=%" PRIu64 ", p=%" PRIu64 ", c=%" PRIu64 ", r=%" PRIu64 ", isd=%d\n", \
      label, pk->table_id, pk->page_id, pk->checkpoint_id, pk->revision, (int)pk->is_delta)

/*
 * Return a string representing the current match value. Can only be used in single threaded code!
 */
static const char *
ret_match_string(PALM_KV_PAGE_MATCHES *matches)
{
    u_int i;
    static char return_string[256]; /* Return value. */

    for (i = 0; i < matches->size && i + 1 < sizeof(return_string); ++i)
        return_string[i] = ((char *)matches->data)[i];
    return_string[i] = '\0';
    return (return_string);
}
#endif

static uint64_t
palm_kv_timestamp_us(void)
{
    struct timeval v;
    int ret;

    ret = gettimeofday(&v, NULL);
    assert(ret == 0);
    (void)ret; /* Assure that ret is "used" when assertions are not in effect. */

    return (uint64_t)(v.tv_sec * WT_MILLION + v.tv_usec);
}

int
palm_kv_env_create(PALM_KV_ENV **envp, uint32_t cache_size_mb)
{
    PALM_KV_ENV *env;
    int ret;

    env = (PALM_KV_ENV *)calloc(1, sizeof(PALM_KV_ENV));
    if (env == 0)
        return (ENOMEM);
    if ((ret = mdb_env_create(&env->lmdb_env)) != 0) {
        free(env);
        return (ret);
    }
    if ((ret = mdb_env_set_maxdbs(env->lmdb_env, PALM_MAX_DBI)) != 0) {
        free(env);
        return (ret);
    }
    if ((ret = mdb_env_set_mapsize(env->lmdb_env, cache_size_mb * MEGABYTE)) != 0) {
        free(env);
        return (ret);
    }
    *envp = env;
    return (ret);
}

int
palm_kv_env_open(PALM_KV_ENV *env, const char *homedir)
{
    int dead_count, ret;
    MDB_txn *txn;

    if ((ret = mdb_env_open(env->lmdb_env, homedir, 0, 0666)) != 0)
        return (ret);

    /* For good multi-process hygiene, this should be called periodically. */
    /* TODO: add this call at checkpoints, or every 10000 calls, etc. */
    if ((ret = mdb_reader_check(env->lmdb_env, &dead_count)) != 0)
        return (ret);
    (void)dead_count;

    if ((ret = mdb_txn_begin(env->lmdb_env, NULL, 0, &txn)) != 0)
        return (ret);

    /* Note: if adding a new named database, increase PALM_MAX_DBI. */
    if ((ret = mdb_dbi_open(txn, "globals", MDB_CREATE | MDB_INTEGERKEY, &env->lmdb_globals_dbi)) !=
      0) {
        mdb_txn_abort(txn);
        return (ret);
    }
    if ((ret = mdb_dbi_open(txn, "tables", MDB_CREATE | MDB_INTEGERKEY, &env->lmdb_tables_dbi)) !=
      0) {
        mdb_txn_abort(txn);
        return (ret);
    }
    if ((ret = mdb_dbi_open(txn, "pages", MDB_CREATE, &env->lmdb_pages_dbi)) != 0) {
        mdb_txn_abort(txn);
        return (ret);
    }
    if ((ret = mdb_txn_commit(txn)) != 0)
        return (ret);

    return (ret);
}

void
palm_kv_env_close(PALM_KV_ENV *env)
{
    (void)mdb_env_close(env->lmdb_env);
    free(env);
}

int
palm_kv_begin_transaction(PALM_KV_CONTEXT *context, PALM_KV_ENV *env, bool readonly)
{
    context->env = env;
    context->lmdb_txn = NULL;
    // TODO: report failures?  For all these funcs
    return (mdb_txn_begin(env->lmdb_env, NULL, readonly ? MDB_RDONLY : 0, &context->lmdb_txn));
}

int
palm_kv_commit_transaction(PALM_KV_CONTEXT *context)
{
    int ret;

    assert(context->lmdb_txn != NULL);
    ret = mdb_txn_commit(context->lmdb_txn);
    context->lmdb_txn = NULL;
    return (ret);
}

void
palm_kv_rollback_transaction(PALM_KV_CONTEXT *context)
{
    assert(context->lmdb_txn != NULL);
    mdb_txn_abort(context->lmdb_txn);
    context->lmdb_txn = NULL;
}

int
palm_kv_put_global(PALM_KV_CONTEXT *context, PALM_KV_GLOBAL_KEY key, uint64_t value)
{
    MDB_val kval;
    MDB_val vval;
    u_int k;

    assert(context->lmdb_txn != NULL);

    memset(&kval, 0, sizeof(kval));
    memset(&vval, 0, sizeof(kval));

    k = (u_int)key;
    if (value > UINT_MAX)
        return (EINVAL);

    kval.mv_size = sizeof(k);
    kval.mv_data = &k;
    vval.mv_size = sizeof(value);
    vval.mv_data = &value;
    return (mdb_put(context->lmdb_txn, context->env->lmdb_globals_dbi, &kval, &vval, 0));
}

int
palm_kv_get_global(PALM_KV_CONTEXT *context, PALM_KV_GLOBAL_KEY key, uint64_t *valuep)
{
    MDB_val kval;
    MDB_val vval;
    u_int k;
    int ret;

    assert(context->lmdb_txn != NULL);

    memset(&kval, 0, sizeof(kval));
    memset(&vval, 0, sizeof(kval));
    k = (u_int)key;

    kval.mv_size = sizeof(k);
    kval.mv_data = &k;
    ret = mdb_get(context->lmdb_txn, context->env->lmdb_globals_dbi, &kval, &vval);
    if (ret == 0) {
        if (vval.mv_size != sizeof(uint64_t))
            return (EIO); /* not expected, data damaged, could be assert */
        *valuep = *(uint64_t *)vval.mv_data;
    }
    return (ret);
}

int
palm_kv_put_page(PALM_KV_CONTEXT *context, uint64_t table_id, uint64_t page_id,
  uint64_t checkpoint_id, uint64_t revision, bool is_delta, uint64_t backlink, uint64_t base,
  uint32_t flags, const WT_ITEM *buf)
{
    MDB_val kval;
    MDB_val vval;
    PAGE_KEY page_key;

    memset(&kval, 0, sizeof(kval));
    memset(&vval, 0, sizeof(kval));
    page_key.table_id = table_id;
    page_key.page_id = page_id;
    page_key.checkpoint_id = checkpoint_id;
    page_key.revision = revision;
    page_key.is_delta = is_delta;
    page_key.backlink = backlink;
    page_key.base = base;
    page_key.flags = flags;
    page_key.timestamp_materialized_us = palm_kv_timestamp_us() + context->materialization_delay_us;
    swap_page_key(&page_key, &page_key);
    kval.mv_size = sizeof(page_key);
    kval.mv_data = &page_key;
    vval.mv_size = buf->size;
    vval.mv_data = (void *)buf->data;

    return (mdb_put(context->lmdb_txn, context->env->lmdb_pages_dbi, &kval, &vval, 0));
}

int
palm_kv_get_page_matches(PALM_KV_CONTEXT *context, uint64_t table_id, uint64_t page_id,
  uint64_t checkpoint_id, PALM_KV_PAGE_MATCHES *matches)
{
    MDB_val kval;
    MDB_val vval;
    PAGE_KEY page_key;
    PAGE_KEY result_key;
    PAGE_KEY *readonly_result_key;
    uint64_t now;
    int ret;

    memset(&kval, 0, sizeof(kval));
    memset(&vval, 0, sizeof(vval));
    memset(matches, 0, sizeof(*matches));
    memset(&page_key, 0, sizeof(page_key));
    readonly_result_key = NULL;
    now = palm_kv_timestamp_us();

    matches->table_id = table_id;
    matches->page_id = page_id;
    matches->checkpoint_id = checkpoint_id;

    page_key.table_id = table_id;
    page_key.page_id = page_id;
    page_key.checkpoint_id = checkpoint_id;
    page_key.revision = UINT64_MAX;
    swap_page_key(&page_key, &page_key);
    kval.mv_size = sizeof(page_key);
    kval.mv_data = &page_key;
    if ((ret = mdb_cursor_open(
           context->lmdb_txn, context->env->lmdb_pages_dbi, &matches->lmdb_cursor)) != 0)
        return (ret);
    ret = mdb_cursor_get(matches->lmdb_cursor, &kval, &vval, MDB_SET_RANGE);
    if (ret == MDB_NOTFOUND) {
        /* If we went off the end, go to the last record. */
        ret = mdb_cursor_get(matches->lmdb_cursor, &kval, &vval, MDB_LAST);
    }
    if (ret == 0) {
        if (kval.mv_size != sizeof(PAGE_KEY))
            return (EIO); /* not expected, data damaged, could be assert */
        readonly_result_key = (PAGE_KEY *)kval.mv_data;
        swap_page_key(readonly_result_key, &result_key);
    }
    /*
     * Now back up until we get a match. This will be the last valid record that matches the
     * table/page.
     */
    while (ret == 0 && !RESULT_MATCH(&result_key, table_id, page_id, now)) {
        ret = mdb_cursor_get(matches->lmdb_cursor, &kval, &vval, MDB_PREV);
        readonly_result_key = (PAGE_KEY *)kval.mv_data;
        swap_page_key(readonly_result_key, &result_key);
    }
    /*
     * Now back up until we match table/page/checkpoint.
     */
    while (ret == 0 && RESULT_MATCH(&result_key, table_id, page_id, now) &&
      result_key.checkpoint_id >= checkpoint_id) {

        /* If this is what we're looking for, we're done, and the cursor is positioned. */
        /* TODO: maybe can't happen, with SET_RANGE. */
        if (result_key.checkpoint_id == checkpoint_id && result_key.is_delta == false) {
            matches->size = vval.mv_size;
            matches->data = vval.mv_data;
            matches->first = true;
            return (0);
        }
        ret = mdb_cursor_get(matches->lmdb_cursor, &kval, &vval, MDB_PREV);
        readonly_result_key = (PAGE_KEY *)kval.mv_data;
        swap_page_key(readonly_result_key, &result_key);
    }
    if (ret == MDB_NOTFOUND) {
        /* We're done, there are no matches. */
        mdb_cursor_close(matches->lmdb_cursor);
        matches->lmdb_cursor = NULL;
        return (0);
    }
    matches->error = ret;
    return (ret);
}

bool
palm_kv_next_page_match(PALM_KV_PAGE_MATCHES *matches)
{
    MDB_val kval;
    MDB_val vval;
    PAGE_KEY *readonly_page_key;
    PAGE_KEY page_key;
    uint64_t now;
    int ret;

    if (matches->lmdb_cursor == NULL)
        return (false);

    now = palm_kv_timestamp_us();

    memset(&page_key, 0, sizeof(page_key));
    memset(&kval, 0, sizeof(kval));
    memset(&vval, 0, sizeof(vval));
    if (matches->first) {
        /*
         * We already have the value set from the positioning. Return the value, and set up to
         * advance the next time.
         */
        matches->first = false;
        return (true);
    }

    ret = mdb_cursor_get(matches->lmdb_cursor, &kval, &vval, MDB_NEXT);
    if (ret == 0) {
        if (kval.mv_size != sizeof(PAGE_KEY))
            return (EIO); /* not expected, data damaged, could be assert */
        readonly_page_key = (PAGE_KEY *)kval.mv_data;
        swap_page_key(readonly_page_key, &page_key);

        if (RESULT_MATCH(&page_key, matches->table_id, matches->page_id, now) &&
          page_key.checkpoint_id == matches->checkpoint_id) {
            matches->size = vval.mv_size;
            matches->data = vval.mv_data;
            matches->revision = page_key.revision;
            matches->backlink = page_key.backlink;
            matches->base = page_key.base;
            matches->flags = page_key.flags;
            return (true);
        }
    }

    /* There are no more matches, or there was an error, so close the cursor. */
    mdb_cursor_close(matches->lmdb_cursor);
    matches->lmdb_cursor = NULL;
    if (ret != MDB_NOTFOUND)
        matches->error = ret;
    return (false);
}
