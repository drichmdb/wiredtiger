/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#pragma once

/* cp_control_point_gen.h: Generated declarations for control points. */
/* In the future this file will be generated. Until then this file must be edited when a new control
 * point is created. */

#ifdef HAVE_CONTROL_POINTS

/*
 * The name below is for a per connection control point named "Example control point".
 *
 * Each per connection control point has:
 * - Per connection control point ID (Could be generated):
 * WT_CONN_CONTROL_POINT_ID_EXAMPLE_CONTROL_POINT.
 *
 * The name below is for a per session control point named "Example control point2".
 *
 * Each per session control point has:
 * - Per session control point ID (Could be generated):
 * WT_CONN_CONTROL_POINT_ID_EXAMPLE_CONTROL_POINT2.
 */

#include "control_points.h"

/*
 * Per connection control point IDs.
 */
#define WT_CONN_CONTROL_POINT_ID_MAIN_START_PRINTING ((wt_control_point_id_t)0)
#define WT_CONN_CONTROL_POINT_ID_THREAD_0 ((wt_control_point_id_t)1)
#define WT_CONN_CONTROL_POINT_ID_THREAD_1 ((wt_control_point_id_t)2)
#define WT_CONN_CONTROL_POINT_ID_THREAD_2 ((wt_control_point_id_t)3)
#define WT_CONN_CONTROL_POINT_ID_THREAD_3 ((wt_control_point_id_t)4)
#define WT_CONN_CONTROL_POINT_ID_THREAD_4 ((wt_control_point_id_t)5)
#define WT_CONN_CONTROL_POINT_ID_THREAD_5 ((wt_control_point_id_t)6)
#define WT_CONN_CONTROL_POINT_ID_THREAD_6 ((wt_control_point_id_t)7)
#define WT_CONN_CONTROL_POINT_ID_THREAD_7 ((wt_control_point_id_t)8)
#define WT_CONN_CONTROL_POINT_ID_THREAD_8 ((wt_control_point_id_t)9)
#define WT_CONN_CONTROL_POINT_ID_THREAD_9 ((wt_control_point_id_t)10)

/* The number of per connection control points (Could be generated). */
#define CONNECTION_CONTROL_POINTS_SIZE 11

/*
 * Per session control point IDs.
 */
#if 0 /* For example */
#define WT_SESSION_CONTROL_POINT_ID_EXAMPLE2 ((wt_control_point_id_t)0)
#endif

/* The number of per session control points (Could be generated). */
#define SESSION_CONTROL_POINTS_SIZE 0

#endif /* HAVE_CONTROL_POINTS */
