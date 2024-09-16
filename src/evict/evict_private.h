/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#pragma once

/*
 * Tuning constants: I hesitate to call this tuning, but we want to review some number of pages from
 * each file's in-memory tree for each page we evict.
 */
#define WT_EVICT_MAX_TREES WT_THOUSAND /* Maximum walk points */
#define WT_EVICT_WALK_BASE 300         /* Pages tracked across file visits */
#define WT_EVICT_WALK_INCR 100         /* Pages added each walk */

/*
 * WT_EVICT_ENTRY --
 *	Encapsulation of an eviction candidate.
 */
struct __wt_evict_entry {
    WT_BTREE *btree; /* Enclosing btree object */
    WT_REF *ref;     /* Page to flush/evict */
    uint64_t score;  /* Relative eviction priority */
};

#define WT_EVICT_URGENT_QUEUE 2 /* Urgent queue index */

/*
 * WT_EVICT_QUEUE --
 *	Encapsulation of an eviction candidate queue.
 */
struct __wt_evict_queue {
    WT_SPINLOCK evict_lock;                /* Eviction LRU queue */
    WT_EVICT_ENTRY *evict_queue;           /* LRU pages being tracked */
    WT_EVICT_ENTRY *evict_current;         /* LRU current page to be evicted */
    uint32_t evict_candidates;             /* LRU list pages to evict */
    uint32_t evict_entries;                /* LRU entries in the queue */
    wt_shared volatile uint32_t evict_max; /* LRU maximum eviction slot used */
};

struct __wt_evict_priv {
    uint64_t last_eviction_progress; /* Tracked eviction progress */
    struct timespec stuck_time;      /* Stuck time */

    /*
     * Eviction thread information.
     */
    WT_CONDVAR *evict_cond;      /* Eviction server condition */
    WT_SPINLOCK evict_walk_lock; /* Eviction walk location */

    uint64_t cache_max_wait_us;      /* Maximum time an operation waits for space in cache */
    uint64_t cache_stuck_timeout_ms; /* Maximum time the cache can be stuck for in diagnostic mode
                                        before timing out */

    /*
     * Eviction thread tuning information.
     */
    uint32_t evict_tune_datapts_needed;          /* Data needed to tune */
    struct timespec evict_tune_last_action_time; /* Time of last action */
    struct timespec evict_tune_last_time;        /* Time of last check */
    uint32_t evict_tune_num_points;              /* Number of values tried */
    uint64_t evict_tune_progress_last;           /* Progress counter */
    uint64_t evict_tune_progress_rate_max;       /* Max progress rate */
    bool evict_tune_stable;                      /* Are we stable? */
    uint32_t evict_tune_workers_best;            /* Best performing value */

    /*
     * LRU eviction list information.
     */
    WT_SPINLOCK evict_pass_lock;   /* Eviction pass lock */
    WT_SESSION_IMPL *walk_session; /* Eviction pass session */

    WT_SPINLOCK evict_queue_lock;        /* Eviction current queue lock */
#define WT_EVICT_QUEUE_MAX 3             /* Two ordinary queues plus urgent */
    WT_EVICT_QUEUE *evict_queues;        /* len WT_EVICT_QUEUE_MAX */
    WT_EVICT_QUEUE *evict_current_queue; /* LRU current queue in use */
    WT_EVICT_QUEUE *evict_fill_queue;    /* LRU next queue to fill.
                                            This is usually the same as the
                                            "other" queue but under heavy
                                            load the eviction server will
                                            start filling the current queue
                                            before it switches. */
    WT_EVICT_QUEUE *evict_other_queue;   /* LRU queue not in use */
    WT_EVICT_QUEUE *evict_urgent_queue;  /* LRU urgent queue */
    uint32_t evict_slots;                /* LRU list eviction slots */

#define WT_EVICT_SCORE_BUMP 10

    /*
     * Score of how often LRU queues are empty on refill. This score varies between 0 (if the queue
     * hasn't been empty for a long time) and 100 (if the queue has been empty the last 10 times we
     * filled up.
     */
    uint32_t evict_empty_score;
};

#define WT_WITH_PASS_LOCK(session, op)                                                         \
    do {                                                                                       \
        WT_WITH_LOCK_WAIT(session, &evict->priv->evict_pass_lock, WT_SESSION_LOCKED_PASS, op); \
    } while (0)

/* DO NOT EDIT: automatically built by prototypes.py: BEGIN */

extern void __wti_evict_list_clear_page(WT_SESSION_IMPL *session, WT_REF *ref);

#ifdef HAVE_UNITTEST

#endif

/* DO NOT EDIT: automatically built by prototypes.py: END */
