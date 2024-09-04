#pragma once

/* DO NOT EDIT: automatically built by prototypes.py: BEGIN */

extern bool __wt_fsync_background_chk(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern bool __wt_handle_is_open(WT_SESSION_IMPL *session, const char *name, bool locked)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern const char *__wt_strerror(WT_SESSION_IMPL *session, int error, char *errbuf, size_t errlen)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_calloc(WT_SESSION_IMPL *session, size_t number, size_t size, void *retp)
  WT_GCC_FUNC_DECL_ATTRIBUTE((visibility("default")))
    WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_close(WT_SESSION_IMPL *session, WT_FH **fhp)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_close_connection_close(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_copy_and_sync(WT_SESSION *wt_session, const char *from, const char *to)
  WT_GCC_FUNC_DECL_ATTRIBUTE((visibility("default")))
    WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_errno(void) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_ext_map_windows_error(WT_EXTENSION_API *wt_api, WT_SESSION *wt_session,
  uint32_t windows_error) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_file_zero(WT_SESSION_IMPL *session, WT_FH *fh, wt_off_t start_off, wt_off_t size,
  WT_THROTTLE_TYPE type) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_filename(WT_SESSION_IMPL *session, const char *name, char **path)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_filename_construct(WT_SESSION_IMPL *session, const char *path,
  const char *file_prefix, uintmax_t id_1, uint32_t id_2, WT_ITEM *buf)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_fopen(WT_SESSION_IMPL *session, const char *name, uint32_t open_flags,
  uint32_t flags, WT_FSTREAM **fstrp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_fsync_background(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_getopt(const char *progname, int nargc, char *const *nargv, const char *ostr)
  WT_GCC_FUNC_DECL_ATTRIBUTE((visibility("default")))
    WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_malloc(WT_SESSION_IMPL *session, size_t bytes_to_allocate, void *retp)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_memdup(WT_SESSION_IMPL *session, const void *str, size_t len, void *retp)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_open(WT_SESSION_IMPL *session, const char *name, WT_FS_OPEN_FILE_TYPE file_type,
  u_int flags, WT_FH **fhp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_os_inmemory(WT_SESSION_IMPL *session)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_realloc(WT_SESSION_IMPL *session, size_t *bytes_allocated_ret,
  size_t bytes_to_allocate, void *retp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_realloc_aligned(WT_SESSION_IMPL *session, size_t *bytes_allocated_ret,
  size_t bytes_to_allocate, void *retp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_realloc_noclear(WT_SESSION_IMPL *session, size_t *bytes_allocated_ret,
  size_t bytes_to_allocate, void *retp) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_remove_if_exists(WT_SESSION_IMPL *session, const char *name, bool durable)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_remove_locked(WT_SESSION_IMPL *session, const char *name, bool *removed)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern int __wt_strndup(WT_SESSION_IMPL *session, const void *str, size_t len, void *retp)
  WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern uint64_t __wt_strtouq(const char *nptr, char **endptr, int base) WT_GCC_FUNC_DECL_ATTRIBUTE(
  (visibility("default"))) WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result));
extern void __wt_abort(WT_SESSION_IMPL *session) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn))
  WT_GCC_FUNC_DECL_ATTRIBUTE((visibility("default")));
extern void __wt_free_int(WT_SESSION_IMPL *session, const void *p_arg)
  WT_GCC_FUNC_DECL_ATTRIBUTE((visibility("default")));
extern void __wt_os_stdio(WT_SESSION_IMPL *session);

#ifdef HAVE_UNITTEST

#endif

/* DO NOT EDIT: automatically built by prototypes.py: END */
