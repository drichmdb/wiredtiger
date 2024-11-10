/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __evict_force_check --
 *     Check if a page matches the criteria for forced eviction.
 */
static bool
__evict_force_check(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_BTREE *btree;
    WT_PAGE *page;
    size_t footprint;

    btree = S2BT(session);
    page = ref->page;

    /* Leaf pages only. */
    if (F_ISSET(ref, WT_REF_FLAG_INTERNAL))
        return (false);

    /*
     * It's hard to imagine a page with a huge memory footprint that has never been modified, but
     * check to be sure.
     */
    if (__wt_page_evict_clean(page))
        return (false);

    /*
     * Exclude the disk image size from the footprint checks.  Usually the
     * disk image size is small compared with the in-memory limit (e.g.
     * 16KB vs 5MB), so this doesn't make a big difference.  Where it is
     * important is for pages with a small number of large values, where
     * the disk image size takes into account large values that have
     * already been written and should not trigger forced eviction.
     */
    footprint = page->memory_footprint;
    if (page->dsk != NULL)
        footprint -= page->dsk->mem_size;

    /* Pages are usually small enough, check that first. */
    if (footprint < btree->splitmempage)
        return (false);

    /*
     * If this session has more than one hazard pointer, eviction will fail and there is no point
     * trying.
     */
    if (__wt_hazard_count(session, ref) > 1)
        return (false);

    /*
     * If the page is less than the maximum size and can be split in-memory, let's try that first
     * without forcing the page to evict on release.
     */
    if (footprint < btree->maxmempage) {
        if (__wt_leaf_page_can_split(session, page))
            return (true);
        return (false);
    }

    /* Bump the oldest ID, we're about to do some visibility checks. */
    WT_IGNORE_RET(__wt_txn_update_oldest(session, 0));

    /*
     * Allow some leeway if the transaction ID isn't moving forward since it is unlikely eviction
     * will be able to evict the page. Don't keep skipping the page indefinitely or large records
     * can lead to extremely large memory footprints.
     */
    if (!__wt_page_evict_retry(session, page))
        return (false);

    /* Trigger eviction on the next page release. */
    __wt_page_evict_soon(session, ref);

    /* If eviction cannot succeed, don't try. */
    return (__wt_page_can_evict(session, ref, NULL));
}

/*
 * __bt_reconstruct_delta --
 *     Reconstruct delta on a page
 */
static int
__bt_reconstruct_delta(WT_SESSION_IMPL *session, WT_REF *ref, WT_ITEM *delta)
{
    WT_CELL_UNPACK_DELTA unpack;
    WT_CURSOR_BTREE cbt;
    WT_DECL_RET;
    WT_DELTA_HEADER *header;
    WT_ITEM key, value;
    WT_PAGE *page;
    WT_ROW *rip;
    WT_UPDATE *first_upd, *upd, *standard_value, *tombstone;
    size_t size, tmp_size, total_size;

    header = (WT_DELTA_HEADER *)delta->data;
    tmp_size = total_size = 0;
    page = ref->page;

    WT_CLEAR(unpack);

    __wt_btcur_init(session, &cbt);
    __wt_btcur_open(&cbt);

    WT_CELL_FOREACH_DELTA(session, header, unpack)
    {
        key.data = unpack.key;
        key.size = unpack.key_size;
        upd = standard_value = tombstone = NULL;
        size = 0;

        /* Search the page and apply the modification. */
        WT_ERR(__wt_row_search(&cbt, &key, true, ref, true, NULL));
        /*
         * We apply deltas from newest to oldest, ignore keys that have already got a delta update.
         */
        if (cbt.compare == 0) {
            if (cbt.ins != NULL) {
                if (cbt.ins->upd != NULL && F_ISSET(cbt.ins->upd, WT_UPDATE_RESTORED_FROM_DELTA))
                    continue;
            } else {
                rip = &page->pg_row[cbt.slot];
                first_upd = WT_ROW_UPDATE(page, rip);
                if (first_upd != NULL && F_ISSET(first_upd, WT_UPDATE_RESTORED_FROM_DELTA))
                    continue;
            }
        }

        if (F_ISSET(&unpack, WT_DELTA_IS_DELETE)) {
            WT_ERR(__wt_upd_alloc_tombstone(session, &tombstone, &tmp_size));
            F_SET(tombstone, WT_UPDATE_DURABLE | WT_UPDATE_RESTORED_FROM_DELTA);
            size += tmp_size;
            upd = tombstone;
        } else {
            value.data = unpack.value;
            value.size = unpack.value_size;
            WT_ERR(__wt_upd_alloc(session, &value, WT_UPDATE_STANDARD, &standard_value, &tmp_size));
            standard_value->txnid = unpack.tw.start_txn;
            standard_value->start_ts = unpack.tw.start_ts;
            standard_value->durable_ts = unpack.tw.durable_start_ts;
            F_SET(standard_value, WT_UPDATE_DURABLE | WT_UPDATE_RESTORED_FROM_DELTA);
            size += tmp_size;

            if (WT_TIME_WINDOW_HAS_STOP(&unpack.tw)) {
                WT_ERR(__wt_upd_alloc_tombstone(session, &tombstone, &tmp_size));
                tombstone->txnid = unpack.tw.stop_txn;
                tombstone->start_ts = unpack.tw.stop_ts;
                tombstone->durable_ts = unpack.tw.durable_stop_ts;
                F_SET(tombstone, WT_UPDATE_DURABLE | WT_UPDATE_RESTORED_FROM_DELTA);
                size += tmp_size;
                tombstone->next = standard_value;
                upd = tombstone;
            } else
                upd = standard_value;
        }

        WT_ERR(__wt_row_modify(&cbt, &key, NULL, &upd, WT_UPDATE_INVALID, true, true));

        total_size += size;
    }
    WT_CELL_FOREACH_END;

    /*
     * The data is written to the disk so we can mark the page clean after re-instantiating prepared
     * updates to avoid reconciling the page every time.
     */
    __wt_page_modify_clear(session, page);
    __wt_cache_page_inmem_incr(session, page, total_size);

    if (0) {
err:
        __wt_free(session, standard_value);
        __wt_free(session, tombstone);
    }
    WT_TRET(__wt_btcur_close(&cbt, true));
    return (ret);
}

/*
 * __bt_reconstruct_deltas --
 *     Reconstruct deltas on a page
 */
static int
__bt_reconstruct_deltas(WT_SESSION_IMPL *session, WT_REF *ref, WT_ITEM *deltas, size_t delta_size)
{
    int i;

    /*
     * We apply the order in reverse order because we only care about the latest change of a key.
     * The older changes are ignore.
     *
     * TODO: this is not the optimal algorithm. We can optimize this by using a min heap.
     */
    for (i = (int)delta_size - 1; i >= 0; --i)
        WT_RET(__bt_reconstruct_delta(session, ref, &deltas[i]));

    return (0);
}

/*
 * __page_read --
 *     Read a page from the file.
 */
static int
__page_read(WT_SESSION_IMPL *session, WT_REF *ref, uint32_t flags)
{
    WT_ADDR_COPY addr;
    WT_DECL_RET;
    WT_ITEM *deltas;
    WT_ITEM *tmp;
    WT_PAGE *notused;
    WT_PAGE_BLOCK_META block_meta;
    size_t count, i;
    uint32_t page_flags;
    uint8_t previous_state;
    bool instantiate_upd;

    WT_CLEAR(block_meta);
    tmp = NULL;
    count = 0;

    /* Lock the WT_REF. */
    switch (previous_state = WT_REF_GET_STATE(ref)) {
    case WT_REF_DISK:
    case WT_REF_DELETED:
        if (WT_REF_CAS_STATE(session, ref, previous_state, WT_REF_LOCKED))
            break;
        return (0);
    default:
        return (0);
    }

    /*
     * Set the WT_REF_FLAG_READING flag for normal reads; this causes reconciliation of the parent
     * page to skip examining this page in detail and write out a reference to the on-disk version.
     * Don't do this for deleted pages, as the reconciliation needs to examine the page delete
     * information. That requires locking the ref, which requires waiting for the read to finish.
     * (It is possible that always writing out a reference to the on-disk version of the page is
     * sufficient in this case, but it's not entirely clear; we expect reads of deleted pages to be
     * rare, so it's better to do the safe thing.)
     */
    if (previous_state == WT_REF_DISK)
        F_SET_ATOMIC_8(ref, WT_REF_FLAG_READING);

    /*
     * Get the address: if there is no address, the page was deleted and a subsequent search or
     * insert is forcing re-creation of the name space. There can't be page delete information,
     * because that information is an amendment to an on-disk page; when a page is deleted any page
     * delete information should expire and be removed before the original on-disk page is actually
     * discarded.
     */
    if (!__wt_ref_addr_copy(session, ref, &addr)) {
        WT_ASSERT(session, previous_state == WT_REF_DELETED);
        WT_ASSERT(session, ref->page_del == NULL);
        WT_ERR(__wti_btree_new_leaf_page(session, ref));
        goto skip_read;
    }

    /*
     * If the page is deleted and the deletion is globally visible, don't bother reading and
     * explicitly instantiating the existing page. Get a fresh page and pretend we got it by reading
     * the on-disk page. Note that it's important to set the instantiated flag on the page so that
     * reconciling the parent internal page knows it was previously deleted. Otherwise it's possible
     * to write out a reference to the original page without the deletion, which will cause it to
     * come back to life unexpectedly.
     *
     * Setting the instantiated flag requires a modify structure. We don't need to mark it dirty; if
     * it gets discarded before something else modifies it, eviction will see the instantiated flag
     * and set the ref state back to WT_REF_DELETED.
     *
     * Skip this optimization in cases that need the obsolete values. To minimize the number of
     * special cases, use the same test as for skipping instantiation below.
     */
    if (previous_state == WT_REF_DELETED &&
      !F_ISSET(S2BT(session), WT_BTREE_SALVAGE | WT_BTREE_UPGRADE | WT_BTREE_VERIFY)) {
        /*
         * If the deletion has not yet been found to be globally visible (page_del isn't NULL),
         * check if it is now, in case we can in fact avoid reading the page. Hide prepared deletes
         * from this check; if the deletion is prepared we still need to load the page, because the
         * reader might be reading at a timestamp early enough to not conflict with the prepare.
         * Update oldest before checking; we're about to read from disk so it's worth doing some
         * work to avoid that.
         */
        WT_ERR(__wt_txn_update_oldest(session, WT_TXN_OLDEST_STRICT | WT_TXN_OLDEST_WAIT));
        if (ref->page_del != NULL && __wt_page_del_visible_all(session, ref->page_del, true))
            __wt_overwrite_and_free(session, ref->page_del);

        if (ref->page_del == NULL) {
            WT_ERR(__wti_btree_new_leaf_page(session, ref));
            WT_ERR(__wt_page_modify_init(session, ref->page));
            ref->page->modify->instantiated = true;
            goto skip_read;
        }
    }

    /* There's an address, read the backing disk page and build an in-memory version of the page. */
    WT_ERR(__wt_blkcache_read_multi(
      session, &tmp, &count, &block_meta, addr.block_cookie, addr.block_cookie_size));

    WT_ASSERT(session, tmp != NULL && count > 0);

    if (count > 1)
        deltas = &tmp[1];
    else
        deltas = NULL;

    /*
     * Build the in-memory version of the page. Clear our local reference to the allocated copy of
     * the disk image on return, the in-memory object steals it.
     *
     * If a page is read with eviction disabled, we don't count evicting it as progress. Since
     * disabling eviction allows pages to be read even when the cache is full, we want to avoid
     * workloads repeatedly reading a page with eviction disabled (e.g., a metadata page), then
     * evicting that page and deciding that is a sign that eviction is unstuck.
     */
    page_flags = WT_DATA_IN_ITEM(&tmp[0]) ? WT_PAGE_DISK_ALLOC : WT_PAGE_DISK_MAPPED;
    if (LF_ISSET(WT_READ_IGNORE_CACHE_SIZE))
        FLD_SET(page_flags, WT_PAGE_EVICT_NO_PROGRESS);
    if (LF_ISSET(WT_READ_PREFETCH))
        FLD_SET(page_flags, WT_PAGE_PREFETCH);
    WT_ERR(__wti_page_inmem(session, ref, tmp[0].data, page_flags, &notused, &instantiate_upd));
    tmp[0].mem = NULL;
    ref->page->block_meta = block_meta;
    if (instantiate_upd && !WT_IS_HS(session->dhandle))
        WT_ERR(__wti_page_inmem_updates(session, ref));

    /*
     * In the case of a fast delete, move all of the page's records to a deleted state based on the
     * fast-delete information. Skip for special commands that don't care about an in-memory state.
     * (But do set up page->modify and set page->modify->instantiated so evicting the pages while
     * these commands are working doesn't go off the rails.)
     *
     * There are two possible cases: the state was WT_REF_DELETED and page_del was or wasn't NULL.
     * It used to also be possible for eviction to set the state to WT_REF_DISK while the parent
     * page nonetheless had a WT_CELL_ADDR_DEL cell. This is not supposed to happen any more, so for
     * now at least assert it doesn't.
     *
     * page_del gets cleared and set to NULL if the deletion is found to be globally visible; this
     * can happen in any of several places.
     */
    WT_ASSERT(
      session, previous_state != WT_REF_DISK || (ref->page_del == NULL && addr.del_set == false));

    /* Reconstruct deltas*/
    if (count > 1) {
        ret = __bt_reconstruct_deltas(session, ref, deltas, count - 1);
        for (i = 0; i < count - 1; ++i)
            __wt_buf_free(session, &deltas[i]);
        WT_ERR(ret);
    }

    __wt_free(session, tmp);

    if (previous_state == WT_REF_DELETED) {
        if (F_ISSET(S2BT(session), WT_BTREE_SALVAGE | WT_BTREE_UPGRADE | WT_BTREE_VERIFY)) {
            WT_ERR(__wt_page_modify_init(session, ref->page));
            ref->page->modify->instantiated = true;
        } else
            WT_ERR(__wti_delete_page_instantiate(session, ref));
    }

skip_read:
    F_CLR_ATOMIC_8(ref, WT_REF_FLAG_READING);
    WT_REF_SET_STATE(ref, WT_REF_MEM);

    WT_ASSERT(session, ret == 0);
    return (0);

err:
    /*
     * If the function building an in-memory version of the page failed, it discarded the page, but
     * not the disk image. Discard the page and separately discard the disk image in all cases.
     */
    if (ref->page != NULL)
        __wt_ref_out(session, ref);

    F_CLR_ATOMIC_8(ref, WT_REF_FLAG_READING);
    WT_REF_SET_STATE(ref, previous_state);

    if (tmp != NULL) {
        for (i = 0; i < count; ++i)
            __wt_buf_free(session, &tmp[i]);
        __wt_free(session, tmp);
    }

    return (ret);
}

/*
 * __wt_page_in_func --
 *     Acquire a hazard pointer to a page; if the page is not in-memory, read it from the disk and
 *     build an in-memory version.
 */
int
__wt_page_in_func(WT_SESSION_IMPL *session, WT_REF *ref, uint32_t flags
#ifdef HAVE_DIAGNOSTIC
  ,
  const char *func, int line
#endif
)
{
    WT_BTREE *btree;
    WT_DECL_RET;
    WT_PAGE *page;
    WT_TXN *txn;
    uint64_t sleep_usecs, yield_cnt;
    uint8_t current_state;
    int force_attempts;
    bool busy, cache_work, evict_skip, stalled, wont_need;

    btree = S2BT(session);
    txn = session->txn;

    if (F_ISSET(session, WT_SESSION_IGNORE_CACHE_SIZE))
        LF_SET(WT_READ_IGNORE_CACHE_SIZE);

    /*
     * Ignore reads of pages already known to be in cache, otherwise the eviction server can
     * dominate these statistics.
     */
    if (!LF_ISSET(WT_READ_CACHE))
        WT_STAT_CONN_DSRC_INCR(session, cache_pages_requested);

    if (LF_ISSET(WT_READ_PREFETCH))
        WT_STAT_CONN_INCR(session, cache_pages_prefetch);

    /*
     * If configured, free stashed memory more aggressively to encourage finding bugs in generation
     * tracking code.
     */
    if (FLD_ISSET(S2C(session)->timing_stress_flags, WT_TIMING_STRESS_AGGRESSIVE_STASH_FREE))
        __wt_stash_discard(session);

    for (evict_skip = stalled = wont_need = false, force_attempts = 0, sleep_usecs = yield_cnt = 0;
         ;) {
        switch (current_state = WT_REF_GET_STATE(ref)) {
        case WT_REF_DELETED:
            /* Optionally limit reads to cache-only. */
            if (LF_ISSET(WT_READ_CACHE | WT_READ_NO_WAIT))
                return (WT_NOTFOUND);
            if (LF_ISSET(WT_READ_SKIP_DELETED) &&
              __wti_delete_page_skip(session, ref, !F_ISSET(txn, WT_TXN_HAS_SNAPSHOT)))
                return (WT_NOTFOUND);
            goto read;
        case WT_REF_DISK:
            /* Optionally limit reads to cache-only. */
            if (LF_ISSET(WT_READ_CACHE))
                return (WT_NOTFOUND);
read:
            /*
             * The page isn't in memory, read it. If this thread respects the cache size, check for
             * space in the cache.
             */
            if (!LF_ISSET(WT_READ_IGNORE_CACHE_SIZE))
                WT_RET(__wt_cache_eviction_check(session, true, txn->mod_count == 0, NULL));
            WT_RET(__page_read(session, ref, flags));

            /* We just read a page, don't evict it before we have a chance to use it. */
            evict_skip = true;
            FLD_CLR(session->dhandle->advisory_flags, WT_DHANDLE_ADVISORY_EVICTED);

            /*
             * If configured to not trash the cache, leave the page generation unset, we'll set it
             * before returning to the oldest read generation, so the page is forcibly evicted as
             * soon as possible. We don't do that set here because we don't want to evict the page
             * before we "acquire" it. Also avoid queuing a pre-fetch page for forced eviction
             * before it has a chance of being used. Otherwise the work we've just done is wasted.
             */
            wont_need = LF_ISSET(WT_READ_WONT_NEED) ||
              F_ISSET(session, WT_SESSION_READ_WONT_NEED) ||
              (!LF_ISSET(WT_READ_PREFETCH) && F_ISSET(S2C(session)->cache, WT_CACHE_EVICT_NOKEEP));
            continue;
        case WT_REF_LOCKED:
            if (LF_ISSET(WT_READ_NO_WAIT))
                return (WT_NOTFOUND);

            if (F_ISSET_ATOMIC_8(ref, WT_REF_FLAG_READING)) {
                if (LF_ISSET(WT_READ_CACHE))
                    return (WT_NOTFOUND);

                /* Waiting on another thread's read, stall. */
                WT_STAT_CONN_INCR(session, page_read_blocked);
            } else
                /* Waiting on eviction, stall. */
                WT_STAT_CONN_INCR(session, page_locked_blocked);

            stalled = true;
            break;
        case WT_REF_SPLIT:
            return (WT_RESTART);
        case WT_REF_MEM:
            /*
             * The page is in memory.
             *
             * Get a hazard pointer if one is required. We cannot be evicting if no hazard pointer
             * is required, we're done.
             */
            if (F_ISSET(btree, WT_BTREE_NO_EVICT))
                goto skip_evict;

/*
 * The expected reason we can't get a hazard pointer is because the page is being evicted, yield,
 * try again.
 */
#ifdef HAVE_DIAGNOSTIC
            WT_RET(__wt_hazard_set_func(session, ref, &busy, func, line));
#else
            WT_RET(__wt_hazard_set_func(session, ref, &busy));
#endif
            if (busy) {
                WT_STAT_CONN_INCR(session, page_busy_blocked);
                break;
            }

            /*
             * If a page has grown too large, we'll try and forcibly evict it before making it
             * available to the caller. There are a variety of cases where that's not possible.
             * Don't involve a thread resolving a transaction in forced eviction, they're usually
             * making the problem better.
             */
            if (evict_skip || F_ISSET(session, WT_SESSION_RESOLVING_TXN) ||
              LF_ISSET(WT_READ_NO_SPLIT) || btree->evict_disabled > 0 || btree->lsm_primary)
                goto skip_evict;

            /*
             * If reconciliation is disabled (e.g., when inserting into the history store table),
             * skip forced eviction if the page can't split.
             */
            if (F_ISSET(session, WT_SESSION_NO_RECONCILE) &&
              !__wt_leaf_page_can_split(session, ref->page))
                goto skip_evict;

            /*
             * Don't evict if we are operating in a transaction on a checkpoint cursor. Eviction
             * would use the cursor's snapshot, which won't be correct.
             */
            if (F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT))
                goto skip_evict;

            /*
             * Forcibly evict pages that are too big.
             */
            if (force_attempts < 10 && __evict_force_check(session, ref)) {
                ++force_attempts;
                ret = __wt_page_release_evict(session, ref, 0);
                /*
                 * If forced eviction succeeded, don't retry. If it failed, stall.
                 */
                if (ret == 0)
                    evict_skip = true;
                else if (ret == EBUSY) {
                    WT_NOT_READ(ret, 0);
                    /*
                     * Don't back off if the session is configured not to do reconciliation, that
                     * just wastes time for no benefit. Without this check a reconciliation of a
                     * page that requires writing content to the history store can stall trying to
                     * force-evict a history store page when there is no chance it will be evicted.
                     */

                    if (F_ISSET(session, WT_SESSION_NO_RECONCILE)) {
                        WT_STAT_CONN_INCR(session, cache_eviction_force_no_retry);
                        evict_skip = true;
                    } else {
                        WT_STAT_CONN_INCR(session, page_forcible_evict_blocked);
                        stalled = true;
                        break;
                    }
                }
                WT_RET(ret);

                /*
                 * The result of a successful forced eviction is a page-state transition
                 * (potentially to an in-memory page we can use, or a restart return for our
                 * caller), continue the outer page-acquisition loop.
                 */
                continue;
            }

skip_evict:
            page = ref->page;
            /*
             * Keep track of whether a session is reading leaf pages into the cache. This allows for
             * the session to decide whether pre-fetch would be helpful. It might not work if a
             * session has multiple cursors on different tables open, since the operations on
             * different tables get in the way of the heuristic. That isn't super likely - this is
             * to catch traversals through a btree, not complex multi-table user transactions.
             */
            if (!LF_ISSET(WT_READ_PREFETCH) && F_ISSET(ref, WT_REF_FLAG_LEAF)) {
                /*
                 * If the page was read by this retrieval or was pulled into the cache via the
                 * pre-fetch mechanism, count that as a page read directly from disk.
                 */
                if (F_ISSET_ATOMIC_16(page, WT_PAGE_PREFETCH) ||
                  __wt_atomic_load64(&page->read_gen) == WT_READGEN_NOTSET)
                    ++session->pf.prefetch_disk_read_count;
                else
                    session->pf.prefetch_disk_read_count = 0;
            }
            /*
             * If we read the page and are configured to not trash the cache, and no other thread
             * has already used the page, set the read generation so the page is evicted soon.
             *
             * Otherwise, if we read the page, or, if configured to update the page's read
             * generation and the page isn't already flagged for forced eviction, update the page
             * read generation.
             */
            if (__wt_atomic_load64(&page->read_gen) == WT_READGEN_NOTSET) {
                if (wont_need)
                    __wt_atomic_store64(&page->read_gen, WT_READGEN_WONT_NEED);
                else
                    __wt_cache_read_gen_new(session, page);
            } else if (!LF_ISSET(WT_READ_NO_GEN))
                __wt_cache_read_gen_bump(session, page);

            /*
             * Check if we need an autocommit transaction. Starting a transaction can trigger
             * eviction, so skip it if eviction isn't permitted.
             *
             * The logic here is a little weird: some code paths do a blanket ban on checking the
             * cache size in sessions, but still require a transaction (e.g., when updating metadata
             * or the history store). If WT_READ_IGNORE_CACHE_SIZE was passed in explicitly, we're
             * done. If we set WT_READ_IGNORE_CACHE_SIZE because it was set in the session then make
             * sure we start a transaction.
             */
            return (LF_ISSET(WT_READ_IGNORE_CACHE_SIZE) &&
                  !F_ISSET(session, WT_SESSION_IGNORE_CACHE_SIZE) ?
                0 :
                __wt_txn_autocommit_check(session));
        default:
            return (__wt_illegal_value(session, current_state));
        }

        /*
         * We failed to get the page -- yield before retrying, and if we've yielded enough times,
         * start sleeping so we don't burn CPU to no purpose.
         */
        if (yield_cnt < WT_THOUSAND) {
            if (!stalled) {
                ++yield_cnt;
                __wt_yield();
                continue;
            }
            yield_cnt = WT_THOUSAND;
        }

        /*
         * If stalling and this thread is allowed to do eviction work, check if the cache needs help
         * evicting clean pages (don't force a read to do dirty eviction). If we do work for the
         * cache, substitute that for a sleep.
         */
        if (!LF_ISSET(WT_READ_IGNORE_CACHE_SIZE)) {
            WT_RET(__wt_cache_eviction_check(session, true, true, &cache_work));
            if (cache_work)
                continue;
        }
        __wt_spin_backoff(&yield_cnt, &sleep_usecs);
        WT_STAT_CONN_INCRV(session, page_sleep, sleep_usecs);
    }
}
