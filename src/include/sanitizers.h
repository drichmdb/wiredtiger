/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#pragma once

/*
 * Clang and gcc use different mechanisms to detect sanitizers, clang using __has_feature and gcc
 * using __SANITIZE_*. Consolidate these checks into a single *SAN_BUILD pre-processor flag.
 */

/* MSan */
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define MSAN_BUILD 1
#endif
#endif

#if defined(__SANITIZE_MEMORY__)
#define MSAN_BUILD 1
#endif

/* TSan */
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TSAN_BUILD 1
#endif
#endif

#if defined(__SANITIZE_THREAD__)
#define TSAN_BUILD 1
#endif

#ifdef MSAN_BUILD
/*
 * FIXME-WT-XXXXX
 * MSan raises false positives on memory initialized by *stat functions. This is fixed in LLVM
 * 14, but until then manually zero the memory to resolve the warnings.
 */
#define DECL_STAT(s) \
    struct stat s;   \
    memset(&s, 0, sizeof(s));
#else
#define DECL_STAT(s) struct stat s;
#endif
