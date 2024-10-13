/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#pragma once

/*
 * __wt_oligarch_log_cmp --
 *     Compare 2 LSNs, return -1 if lsn1 < lsn2, 0if lsn1 == lsn2 and 1 if lsn1 > lsn2.
 */
static WT_INLINE int
__wt_oligarch_log_cmp(WT_LSN *lsn1, WT_LSN *lsn2)
{
    uint64_t l1, l2;

    /*
     * Read LSNs into local variables so that we only read each field once and all comparisons are
     * on the same values.
     */
    WT_READ_ONCE(l1, lsn1->file_offset);
    WT_READ_ONCE(l2, lsn2->file_offset);

    return (l1 < l2 ? -1 : (l1 > l2 ? 1 : 0));
}

/*
 * __wt_oligarch_lsn_offset --
 *     Return a log sequence number's offset.
 */
static WT_INLINE uint32_t
__wt_oligarch_lsn_offset(WT_LSN *lsn)
{
    return (__wt_atomic_load32(&lsn->l.offset));
}

/*
 * __wt_oligarch_log_op --
 *     Return if an operation should be logged.
 */
static WT_INLINE bool
__wt_oligarch_log_op(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);

    if (!F_ISSET(S2BT(session), WT_BTREE_OLIGARCH_LOGGED))
        return (false);

    /*
     * Correct the above check for logging being configured. Files are configured for logging to
     * turn off timestamps, so stop here if there aren't actually any log files.
     */
    if (!FLD_ISSET(conn->oligarch_log_info.log_flags, WT_CONN_LOG_ENABLED))
        return (false);

    /* No logging during recovery. */
    if (F_ISSET(conn, WT_CONN_RECOVERING))
        return (false);

    return (true);
}
