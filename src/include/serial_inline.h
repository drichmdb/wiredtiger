/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * __col_append_serial_func --
 *     Worker function to allocate a record number as necessary, then add a WT_INSERT entry to a
 *     skiplist.
 */
static inline int
__col_append_serial_func(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_INSERT *new_ins,
  uint64_t *recnop, u_int skipdepth)
{
    WT_BTREE *btree;
    uint64_t recno;
    u_int i;

    btree = S2BT(session);

    /*
     * If the application didn't specify a record number, allocate a new one and set up for an
     * append.
     */
    if ((recno = WT_INSERT_RECNO(new_ins)) == WT_RECNO_OOB) {
        recno = WT_INSERT_RECNO(new_ins) = btree->last_recno + 1;
        WT_ASSERT(session,
          WT_SKIP_LAST(cbt->ins_head) == NULL ||
            recno > WT_INSERT_RECNO(WT_SKIP_LAST(cbt->ins_head)));
        for (i = 0; i < skipdepth; i++)
            cbt->ins_stack[i] = cbt->ins_head->tail[i] == NULL ? &cbt->ins_head->head[i] :
                                                                 &cbt->ins_head->tail[i]->next[i];
    }

    /* Confirm position and insert the new WT_INSERT item. */
    WT_RET(__wt_skip_insert_internal__insert(session, NULL, cbt, new_ins, skipdepth, true));

    /*
     * Set the calling cursor's record number. If we extended the file, update the last record
     * number.
     */
    *recnop = recno;

    /*
     * This line is thread-safe. We can only enter this function by holding a lock on the page, and
     * any append that increases last_recno must be appending to the rightmost page in the btree.
     * Ergo, all changes to last_recno are protected by a lock on the rightmost page in the tree.
     */
    if (recno > btree->last_recno)
        btree->last_recno = recno;

    return (0);
}

/*
 * __wt_col_append_serial --
 *     Append a new column-store entry.
 */
static inline int
__wt_col_append_serial(WT_SESSION_IMPL *session, WT_PAGE *page, WT_CURSOR_BTREE *cbt,
  WT_INSERT **new_insp, size_t new_ins_size, uint64_t *recnop, u_int skipdepth, bool exclusive)
{
    WT_DECL_RET;
    WT_INSERT *new_ins;

    /* Clear references to memory we now own and must free on error. */
    new_ins = *new_insp;
    *new_insp = NULL;

    /*
     * Acquire the page's spinlock unless we already have exclusive access. Then call the worker
     * function.
     */
    if (!exclusive)
        WT_PAGE_LOCK(session, page);
    ret = __col_append_serial_func(session, cbt, new_ins, recnop, skipdepth);
    if (!exclusive)
        WT_PAGE_UNLOCK(session, page);

    if (ret != 0) {
        /* Free unused memory on error. */
        __wt_free(session, new_ins);
        return (ret);
    }

    /*
     * Increment in-memory footprint after releasing the mutex: that's safe because the structures
     * we added cannot be discarded while visible to any running transaction, and we're a running
     * transaction, which means there can be no corresponding delete until we complete.
     */
    __wt_cache_page_inmem_incr(session, page, new_ins_size);

    /* Mark the page dirty after updating the footprint. */
    __wt_page_modify_set(session, page);

    return (0);
}

/*
 * __wt_insert_serial --
 *     Insert a row or column-store entry.
 */
static inline int
__wt_insert_serial(WT_SESSION_IMPL *session, WT_PAGE *page, WT_CURSOR_BTREE *cbt,
  WT_INSERT **new_insp, size_t new_ins_size, u_int skipdepth, bool exclusive)
{
    WT_DECL_RET;
    WT_INSERT *new_ins;

    /* Clear references to memory we now own and must free on error. */
    new_ins = *new_insp;
    *new_insp = NULL;

    ret = __wt_skip_insert_internal__insert(
      session, &page->modify->page_lock, cbt, new_ins, skipdepth, exclusive);

    if (ret != 0) {
        /* Free unused memory on error. */
        __wt_free(session, new_ins);
        return (ret);
    }

    /*
     * Increment in-memory footprint after releasing the mutex: that's safe because the structures
     * we added cannot be discarded while visible to any running transaction, and we're a running
     * transaction, which means there can be no corresponding delete until we complete.
     */
    __wt_cache_page_inmem_incr(session, page, new_ins_size);

    /* Mark the page dirty after updating the footprint. */
    __wt_page_modify_set(session, page);

    return (0);
}

/*
 * __wt_update_serial --
 *     Update a row or column-store entry.
 */
static inline int
__wt_update_serial(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_PAGE *page,
  WT_UPDATE **srch_upd, WT_UPDATE **updp, size_t upd_size, bool exclusive)
{
    WT_DECL_RET;
    WT_UPDATE *upd;
    wt_timestamp_t obsolete_timestamp, prev_upd_ts;
    uint64_t txn;

    /* Clear references to memory we now own and must free on error. */
    upd = *updp;
    *updp = NULL;

    WT_ASSERT(session, upd != NULL);

    prev_upd_ts = upd->prev_durable_ts;

    /*
     * All structure setup must be flushed before the structure is entered into the list. We need a
     * write barrier here, our callers depend on it.
     *
     * Swap the update into place. If that fails, a new update was added after our search, we raced.
     * Check if our update is still permitted.
     */
    while (!__wt_atomic_cas_ptr(srch_upd, upd->next, upd)) {
        if ((ret = __wt_txn_modify_check(
               session, cbt, upd->next = *srch_upd, &prev_upd_ts, upd->type)) != 0) {
            /* Free unused memory on error. */
            __wt_free(session, upd);
            return (ret);
        }
    }
    upd->prev_durable_ts = prev_upd_ts;

    /*
     * Increment in-memory footprint after swapping the update into place. Safe because the
     * structures we added cannot be discarded while visible to any running transaction, and we're a
     * running transaction, which means there can be no corresponding delete until we complete.
     */
    __wt_cache_page_inmem_incr(session, page, upd_size);

    /* Mark the page dirty after updating the footprint. */
    __wt_page_modify_set(session, page);

    /*
     * Don't remove obsolete updates in the history store, due to having different visibility rules
     * compared to normal tables. This visibility rule allows different readers to concurrently read
     * globally visible updates, and insert new globally visible updates, due to the reuse of
     * original transaction informations.
     */
    if (WT_IS_HS(session->dhandle))
        return (0);

    /* If there are no subsequent WT_UPDATE structures we are done here. */
    if (upd->next == NULL || exclusive)
        return (0);

    /*
     * We would like to call __wt_txn_update_oldest only in the event that there are further updates
     * to this page, the check against WT_TXN_NONE is used as an indicator of there being further
     * updates on this page.
     */
    if ((txn = page->modify->obsolete_check_txn) != WT_TXN_NONE) {
        obsolete_timestamp = page->modify->obsolete_check_timestamp;
        if (!__wt_txn_visible_all(session, txn, obsolete_timestamp)) {
            /* Try to move the oldest ID forward and re-check. */
            ret = __wt_txn_update_oldest(session, 0);
            /*
             * We cannot proceed if we fail here as we have inserted the updates to the update
             * chain. Panic instead. Currently, we don't ever return any error from
             * __wt_txn_visible_all. We can catch it if we start to do so in the future and properly
             * handle it.
             */
            if (ret != 0)
                WT_RET_PANIC(session, ret, "fail to update oldest after serializing the updates");

            if (!__wt_txn_visible_all(session, txn, obsolete_timestamp))
                return (0);
        }

        page->modify->obsolete_check_txn = WT_TXN_NONE;
    }

    __wt_update_obsolete_check(session, cbt, upd->next, true);

    return (0);
}
