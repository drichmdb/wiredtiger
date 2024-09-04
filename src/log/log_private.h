#pragma once

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
extern int __wti_log_fill(WT_SESSION_IMPL *session, WT_MYSLOT *myslot, bool force, WT_ITEM *record,
  WT_LSN *lsnp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_recover_prevlsn(WT_SESSION_IMPL *session, const uint8_t **pp,
  const uint8_t *end, WT_LSN *lsnp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_release(WT_SESSION_IMPL *session, WT_LOGSLOT *slot, bool *freep)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_slot_switch(WT_SESSION_IMPL *session, WT_MYSLOT *myslot, bool retry,
  bool forced, bool *did_work) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wti_log_system_prevlsn(WT_SESSION_IMPL *session, WT_FH *log_fh, WT_LSN *lsn)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int64_t __wti_log_slot_release(WT_MYSLOT *myslot, int64_t size)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern void __wti_log_slot_activate(WT_SESSION_IMPL *session, WT_LOGSLOT *slot);
extern void __wti_log_slot_join(
  WT_SESSION_IMPL *session, uint64_t mysize, uint32_t flags, WT_MYSLOT *myslot);

#ifdef HAVE_UNITTEST

#endif

/* DO NOT EDIT: automatically built by prototypes.py: END */
