#pragma once

/*
 * We allocate the buffer size, but trigger a slot switch when we cross the maximum size of half the
 * buffer. If a record is more than the buffer maximum then we trigger a slot switch and write that
 * record unbuffered. We use a larger buffer to provide overflow space so that we can switch once we
 * cross the threshold.
 */
#define WT_LOG_SLOT_BUF_SIZE (256 * 1024) /* Must be power of 2 */
#define WT_LOG_SLOT_BUF_MAX ((uint32_t)log->slot_buf_size / 2)
#define WT_LOG_SLOT_UNBUFFERED (WT_LOG_SLOT_BUF_SIZE << 1)

/*
 * Possible values for the consolidation array slot states:
 *
 * WT_LOG_SLOT_CLOSE - slot is in use but closed to new joins.
 *
 * WT_LOG_SLOT_FREE - slot is available for allocation.
 *
 * WT_LOG_SLOT_WRITTEN - slot is written and should be processed by worker.
 *
 * The slot state must be volatile: threads loop checking the state and can't cache the first value
 * they see.
 *
 * The slot state is divided into two 32 bit sizes. One half is the amount joined and the other is
 * the amount released. Since we use a few special states, reserve the top few bits for state. That
 * makes the maximum size less than 32 bits for both joined and released.
 */
/*
 * XXX The log slot bits are signed and should be rewritten as unsigned. For now, give the logging
 * subsystem its own flags macro.
 */
#define FLD_LOG_SLOT_ISSET(field, mask) (((field) & (uint64_t)(mask)) != 0)

/*
 * The high bit is reserved for the special states. If the high bit is set (WT_LOG_SLOT_RESERVED)
 * then we are guaranteed to be in a special state.
 */
#define WT_LOG_SLOT_FREE (-1)    /* Not in use */
#define WT_LOG_SLOT_WRITTEN (-2) /* Slot data written, not processed */

/*
 * If new slot states are added, adjust WT_LOG_SLOT_BITS and WT_LOG_SLOT_MASK_OFF accordingly for
 * how much of the top 32 bits we are using. More slot states here will reduce the maximum size that
 * a slot can hold unbuffered by half. If a record is larger than the maximum we can account for in
 * the slot state we fall back to direct writes.
 */
#define WT_LOG_SLOT_BITS 2
#define WT_LOG_SLOT_MAXBITS (32 - WT_LOG_SLOT_BITS)
#define WT_LOG_SLOT_CLOSE 0x4000000000000000LL    /* Force slot close */
#define WT_LOG_SLOT_RESERVED 0x8000000000000000LL /* Reserved states */

/*
 * Check if the unbuffered flag is set in the joined portion of the slot state.
 */
#define WT_LOG_SLOT_UNBUFFERED_ISSET(state) ((state) & ((int64_t)WT_LOG_SLOT_UNBUFFERED << 32))

#define WT_LOG_SLOT_MASK_OFF 0x3fffffffffffffffLL
#define WT_LOG_SLOT_MASK_ON ~(WT_LOG_SLOT_MASK_OFF)
#define WT_LOG_SLOT_JOIN_MASK (WT_LOG_SLOT_MASK_OFF >> 32)

/*
 * These macros manipulate the slot state and its component parts.
 */
#define WT_LOG_SLOT_FLAGS(state) ((state)&WT_LOG_SLOT_MASK_ON)
#define WT_LOG_SLOT_JOINED(state) (((state)&WT_LOG_SLOT_MASK_OFF) >> 32)
#define WT_LOG_SLOT_JOINED_BUFFERED(state) \
    (WT_LOG_SLOT_JOINED(state) & (WT_LOG_SLOT_UNBUFFERED - 1))
#define WT_LOG_SLOT_JOIN_REL(j, r, s) (((j) << 32) + (r) + (s))
#define WT_LOG_SLOT_RELEASED(state) ((int64_t)(int32_t)(state))
#define WT_LOG_SLOT_RELEASED_BUFFERED(state) \
    ((int64_t)((int32_t)WT_LOG_SLOT_RELEASED(state) & (WT_LOG_SLOT_UNBUFFERED - 1)))

/* Slot is in use */
#define WT_LOG_SLOT_ACTIVE(state) (WT_LOG_SLOT_JOINED(state) != WT_LOG_SLOT_JOIN_MASK)
/* Slot is in use, but closed to new joins */
#define WT_LOG_SLOT_CLOSED(state)                                  \
    (WT_LOG_SLOT_ACTIVE(state) &&                                  \
      (FLD_LOG_SLOT_ISSET((uint64_t)(state), WT_LOG_SLOT_CLOSE) && \
        !FLD_LOG_SLOT_ISSET((uint64_t)(state), WT_LOG_SLOT_RESERVED)))
/* Slot is in use, all data copied into buffer */
#define WT_LOG_SLOT_INPROGRESS(state) (WT_LOG_SLOT_RELEASED(state) != WT_LOG_SLOT_JOINED(state))
#define WT_LOG_SLOT_DONE(state) (WT_LOG_SLOT_CLOSED(state) && !WT_LOG_SLOT_INPROGRESS(state))
/* Slot is in use, more threads may join this slot */
#define WT_LOG_SLOT_OPEN(state)                                           \
    (WT_LOG_SLOT_ACTIVE(state) && !WT_LOG_SLOT_UNBUFFERED_ISSET(state) && \
      !FLD_LOG_SLOT_ISSET((uint64_t)(state), WT_LOG_SLOT_CLOSE) &&        \
      WT_LOG_SLOT_JOINED(state) < WT_LOG_SLOT_BUF_MAX)

struct __wt_logslot {
    WT_CACHE_LINE_PAD_BEGIN
    wt_shared volatile int64_t slot_state; /* Slot state */
    wt_shared int64_t slot_unbuffered;     /* Unbuffered data in this slot */
    wt_shared int slot_error;              /* Error value */
    wt_shared wt_off_t slot_start_offset;  /* Starting file offset */
    wt_shared wt_off_t slot_last_offset;   /* Last record offset */
    WT_LSN slot_release_lsn;               /* Slot release LSN */
    WT_LSN slot_start_lsn;                 /* Slot starting LSN */
    WT_LSN slot_end_lsn;                   /* Slot ending LSN */
    WT_FH *slot_fh;                        /* File handle for this group */
    WT_ITEM slot_buf;                      /* Buffer for grouped writes */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_SLOT_CLOSEFH 0x01u        /* Close old fh on release */
#define WT_SLOT_FLUSH 0x02u          /* Wait for write */
#define WT_SLOT_SYNC 0x04u           /* Needs sync on release */
#define WT_SLOT_SYNC_DIR 0x08u       /* Directory sync on release */
#define WT_SLOT_SYNC_DIRTY 0x10u     /* Sync system buffers on release */
                                     /* AUTOMATIC FLAG VALUE GENERATION STOP 16 */
    wt_shared uint16_t flags_atomic; /* Atomic flags, use F_*_ATOMIC_16 */
    WT_CACHE_LINE_PAD_END
};

#define WT_SLOT_INIT_FLAGS 0

#define WT_SLOT_SYNC_FLAGS (WT_SLOT_SYNC | WT_SLOT_SYNC_DIR | WT_SLOT_SYNC_DIRTY)

#define WT_WITH_SLOT_LOCK(session, log, op)                                            \
    do {                                                                               \
        WT_ASSERT(session, !FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_SLOT));   \
        WT_WITH_LOCK_WAIT(session, &(log)->log_slot_lock, WT_SESSION_LOCKED_SLOT, op); \
    } while (0)

struct __wt_myslot {
    WT_LOGSLOT *slot;    /* Slot I'm using */
    wt_off_t end_offset; /* My end offset in buffer */
    wt_off_t offset;     /* Slot buffer offset */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_MYSLOT_CLOSE 0x1u         /* This thread is closing the slot */
#define WT_MYSLOT_NEEDS_RELEASE 0x2u /* This thread is releasing the slot */
#define WT_MYSLOT_UNBUFFERED 0x4u    /* Write directly */
                                     /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};
typedef struct __wt_myslot WT_MYSLOT;

/* DO NOT EDIT: automatically built by prototypes.py: BEGIN */

extern int __wti_log_acquire(WT_SESSION_IMPL *session, uint64_t recsize, WT_LOGSLOT *slot)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_allocfile(WT_SESSION_IMPL *session, uint32_t lognum, const char *dest)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_close(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_extract_lognum(WT_SESSION_IMPL *session, const char *name, uint32_t *id)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_fill(WT_SESSION_IMPL *session, WT_MYSLOT *myslot, bool force, WT_ITEM *record,
  WT_LSN *lsnp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_open(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_recover_prevlsn(WT_SESSION_IMPL *session, const uint8_t **pp,
  const uint8_t *end, WT_LSN *lsnp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_release(WT_SESSION_IMPL *session, WT_LOGSLOT *slot, bool *freep)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_remove(WT_SESSION_IMPL *session, const char *file_prefix, uint32_t lognum)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_set_version(WT_SESSION_IMPL *session, uint16_t version, uint32_t first_rec,
  bool downgrade, bool live_chg, uint32_t *lognump)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_slot_destroy(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_slot_init(WT_SESSION_IMPL *session, bool alloc)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_slot_switch(WT_SESSION_IMPL *session, WT_MYSLOT *myslot, bool retry,
  bool forced, bool *did_work) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_system_prevlsn(WT_SESSION_IMPL *session, WT_FH *log_fh, WT_LSN *lsn)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int64_t __wti_log_slot_release(WT_MYSLOT *myslot, int64_t size)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern void __wti_log_slot_activate(WT_SESSION_IMPL *session, WT_LOGSLOT *slot);
extern void __wti_log_slot_free(WT_SESSION_IMPL *session, WT_LOGSLOT *slot);
extern void __wti_log_slot_join(
  WT_SESSION_IMPL *session, uint64_t mysize, uint32_t flags, WT_MYSLOT *myslot);
extern void __wti_log_wrlsn(WT_SESSION_IMPL *session, int *yield);

#ifdef HAVE_UNITTEST

#endif

/* DO NOT EDIT: automatically built by prototypes.py: END */
