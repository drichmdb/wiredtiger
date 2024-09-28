/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/* cp_control_point.c: Definitions for control points. */
/* This file must be edited when a new control point is created. */

/*
 * The names below are for a per connection control point named "Example control point".
 *
 * Each per connection control point has:
 * - A choice of action (Must be manual).
 * - A choice of predicate (Must be manual).
 * - Per connection control point data type (Could be generated):
 * __wt_conn_control_point_data_example_control_point.
 * - Per connection control point init function (Could be generated):
 * __wt_conn_control_point_init_example_control_point.
 * - An entry in __wt_conn_control_point_init_all (Could be generated).
 *
 * Each per connection control point that is enabled at startup has:
 * - An entry in __wt_conn_control_point_enable_all (Could be generated).
 *
 * The names below are for a per session control point named "Example control point2".
 *
 * Each per session control point has:
 * - A choice of action (Must be manual).
 * - A choice of predicate (Must be manual).
 * - Per session control point data type (Could be generated):
 * __wt_session_control_point_data_example_control_point2.
 * - Per session control point init function (Could be generated):
 * __wt_session_control_point_init_example_control_point2.
 * - An entry in __wt_session_control_point_init_all (Could be generated).
 *
 * Each per session control point that is enabled at startup has:
 * - An entry in __wt_session_control_point_enable_all (Could be generated).
 */

#include "wt_internal.h"

#ifdef HAVE_CONTROL_POINTS
/*
 * Functions used at the trigger site.
 */
/*
 * __wt_conn_control_point_test_and_trigger --
 *     Test whether a per connection control point is triggered and do common trigger processing. If
 *     disabled or not triggered, return NULL. If triggered, return the control point data. When
 *     done with the data it must be released.
 *
 * @param session The session. @param id The per connection control point's ID.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_test_and_trigger(WT_SESSION_IMPL *session, WT_CONTROL_POINT_ID id)
{
    WT_CONNECTION_IMPL *conn;
    bool triggered;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_CONTROL_POINT *data;

    if (WT_UNLIKELY(id >= CONNECTION_CONTROL_POINTS_SIZE))
        return (NULL);
    conn = S2C(session);
    if (WT_UNLIKELY(conn->control_points == NULL))
        return (NULL);
    cp_registry = &(conn->control_points[id]);

    data = __wt_control_point_get_data(session, cp_registry, false);
    if (data == NULL)
        /* Disabled. */
        return (NULL);
    ++(cp_registry->crossing_count);
    triggered = cp_registry->pred ? cp_registry->pred(session, cp_registry, data) : true;
    if (triggered)
        ++(cp_registry->trigger_count);
    else {
        __wt_control_point_release_data(session, cp_registry, data, false);
        /* Not triggered. */
        data = NULL;
    }
    return (data);
}

/*
 * __wt_session_control_point_test_and_trigger --
 *     Test whether a per session control point is triggered and do common trigger processing. If
 *     disabled or not triggered, return NULL. If triggered, return the control point data. The data
 *     does not need to be released.
 *
 * @param session The session. @param id The per connection control point's ID.
 */
WT_CONTROL_POINT *
__wt_session_control_point_test_and_trigger(WT_SESSION_IMPL *session, WT_CONTROL_POINT_ID id)
{
    bool triggered;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_CONTROL_POINT *data;

    if (WT_UNLIKELY((id >= SESSION_CONTROL_POINTS_SIZE) || (session->control_points == NULL)))
        return (NULL);
    cp_registry = &(session->control_points[id]);

    data = cp_registry->data;
    if (data == NULL)
        /* Disabled. */
        return (NULL);

    triggered = cp_registry->pred ? cp_registry->pred(session, cp_registry, data) : true;
    ++(cp_registry->crossing_count);
    if (triggered)
        ++(cp_registry->trigger_count);
    else
        /* Not triggered. */
        data = NULL;
    return (data);
}

/*
 * Per connection control point initialization.
 */
/* From examples/ex_control_points.c */
/*
 * Per connection control point MainStartPrinting.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_MainStartPrinting {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_MainStartPrinting --
 *     The per connection control point initialization function for control point MainStartPrinting.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_MainStartPrinting(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_MainStartPrinting *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "MainStartPrinting", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD0.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD0 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD0 --
 *     The per connection control point initialization function for control point THREAD0.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD0(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD0 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD0", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD1.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD1 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD1 --
 *     The per connection control point initialization function for control point THREAD1.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD1(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD1 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD1", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD2.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD2 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD2 --
 *     The per connection control point initialization function for control point THREAD2.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD2(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD1 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD2", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD3.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD3 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD3 --
 *     The per connection control point initialization function for control point THREAD3.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD3(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD2 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD3", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD4.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD4 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD4 --
 *     The per connection control point initialization function for control point THREAD4.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD4(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD3 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD4", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD5.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD5 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD5 --
 *     The per connection control point initialization function for control point THREAD5.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD5(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD4 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD5", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD6.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD6 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD6 --
 *     The per connection control point initialization function for control point THREAD6.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD6(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD5 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD6", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD7.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD7 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD7 --
 *     The per connection control point initialization function for control point THREAD7.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD7(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD6 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD7", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD8.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD8 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD8 --
 *     The per connection control point initialization function for control point THREAD8.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD8(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD7 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD8", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Per connection control point THREAD9.
 */
/* Per connection control point data type. */
struct __wt_conn_control_point_data_THREAD9 {
    WT_CONTROL_POINT iface;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER action_data;
};

/* Per connection control point init function. */
/*
 * __wt_conn_control_point_init_THREAD9 --
 *     The per connection control point initialization function for control point THREAD8.
 *
 * @param session The session. @param cp_registry The per connection control point's control point
 *     registry. @param cfg Configuration strings.
 */
WT_CONTROL_POINT *
__wt_conn_control_point_init_THREAD9(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_DECL_RET;
    struct __wt_conn_control_point_data_THREAD8 *init_data;

    ret = __wt_calloc_one(session, &init_data);
    if (WT_UNLIKELY(ret != 0))
        return (NULL);

    /* Initialize configuration parameters. */
    WT_ERR(__wt_control_point_config_action_wait_for_trigger(
      session, (WT_CONTROL_POINT *)init_data, cfg));
    /* The predicate is "Always" therefore no predicate configuration parameters to initialize. */

    /* Extra initialization required for action "Wait for trigger". */
    __wt_control_point_action_init_wait_for_trigger(
      session, "THREAD9", (WT_CONTROL_POINT *)init_data);

err:
    if (ret != 0)
        __wt_free(session, init_data);

    return ((WT_CONTROL_POINT *)init_data);
}

/*
 * Control point startup functions: Initialization.
 */
/*
 * __wt_conn_control_point_init_all --
 *     Initialize all per connection control points. Note, one part of this function must be edited
 *     for each per connection control point.
 *
 * @param session The session.
 */
int
__wt_conn_control_point_init_all(WT_SESSION_IMPL *session)
{
    WT_DECL_RET;
    WT_CONTROL_POINT_REGISTRY *control_points;

    if (CONNECTION_CONTROL_POINTS_SIZE == 0)
        return (0);
    WT_RET(__wt_calloc_def(session, CONNECTION_CONTROL_POINTS_SIZE, &control_points));

    /*
     * This part must be edited. Repeat this for every per connection control point.
     */
    /* From examples/ex_control_points.c */
    control_points[WT_CONN_CONTROL_POINT_ID_MainStartPrinting].init =
      __wt_conn_control_point_init_MainStartPrinting;
    control_points[WT_CONN_CONTROL_POINT_ID_MainStartPrinting].pred = NULL; /* Always */
    WT_ERR(__wt_spin_init(session,
      &(control_points[WT_CONN_CONTROL_POINT_ID_MainStartPrinting].lock), "MainStartPrinting"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_MainStartPrinting].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD0].init = __wt_conn_control_point_init_THREAD0;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD0].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD0].lock), "THREAD0"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD0].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD1].init = __wt_conn_control_point_init_THREAD1;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD1].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD1].lock), "THREAD1"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD1].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD2].init = __wt_conn_control_point_init_THREAD2;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD2].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD2].lock), "THREAD2"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD2].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD3].init = __wt_conn_control_point_init_THREAD3;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD3].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD3].lock), "THREAD3"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD3].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD4].init = __wt_conn_control_point_init_THREAD4;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD4].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD4].lock), "THREAD4"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD4].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD5].init = __wt_conn_control_point_init_THREAD5;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD5].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD5].lock), "THREAD5"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD5].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD6].init = __wt_conn_control_point_init_THREAD6;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD6].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD6].lock), "THREAD6"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD6].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD7].init = __wt_conn_control_point_init_THREAD7;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD7].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD7].lock), "THREAD7"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD7].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD8].init = __wt_conn_control_point_init_THREAD8;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD8].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD8].lock), "THREAD8"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD8].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    control_points[WT_CONN_CONTROL_POINT_ID_THREAD9].init = __wt_conn_control_point_init_THREAD9;
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD9].pred = NULL; /* Always */
    WT_ERR(
      __wt_spin_init(session, &(control_points[WT_CONN_CONTROL_POINT_ID_THREAD9].lock), "THREAD9"));
    /* Extra initialization required for action "Wait for trigger". */
    control_points[WT_CONN_CONTROL_POINT_ID_THREAD9].action_supported =
      WT_CONTROL_POINT_ACTION_ID_WAIT_FOR_TRIGGER;

    /* After all repeats finish with this. */
    S2C(session)->control_points = control_points;

err:
    if (ret != 0)
        __wt_free(session, control_points);
    return (ret);
}

/*
 * __wt_session_control_point_init_all --
 *     Initialize all per session control points. Note, one part of this function must be edited for
 *     each per session control point.
 *
 * @param session The session.
 */
int
__wt_session_control_point_init_all(WT_SESSION_IMPL *session)
{
#if SESSION_CONTROL_POINTS_SIZE == 0
    WT_UNUSED(session);
    return (0);
#else
    WT_DECL_RET;
    WT_CONTROL_POINT_REGISTRY *control_points;

    if (SESSION_CONTROL_POINTS_SIZE == 0)
        return (0);
    WT_RET(__wt_calloc_def(session, SESSION_CONTROL_POINTS_SIZE, &control_points));

    /*
     * This part must be edited. Repeat this for every per session control point.
     */
#if 0 /* For example */
  control_points[WT_SESSION_CONTROL_POINT_ID_EXAMPLE2].init =
      __wt_control_point_session_init_example2;
  control_points[WT_SESSION_CONTROL_POINT_ID_EXAMPLE2].pred =
      __wt_control_point_session_pred_examples;
  WT_ERR(__wt_spin_init(session, &(control_points[WT_SESSION_CONTROL_POINT_ID_EXAMPLE2].lock)));
#endif

    /* After all repeats finish with this. */
    session->control_points = control_points;

err:
    if (ret != 0)
        __wt_free(session, control_points);
    return (ret);
#endif
}

/*
 * Control point startup functions: Enable at startup.
 */
/*
 * __wt_conn_control_point_enable_all --
 *     Enable per connection control points that start enabled. Note, one part of this function must
 *     be edited for each per connection control point that starts enabled.
 *
 * @param conn The connection.
 */
int
__wt_conn_control_point_enable_all(WT_SESSION_IMPL *session, const char **cfg)
{
#if 0 /* If no per connection control points are enabled at the start. */
    WT_UNUSED(session);
    WT_UNUSED(cfg);
#else
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *control_points;

    if (CONNECTION_CONTROL_POINTS_SIZE == 0)
        return (0);
    conn = S2C(session);
    control_points = conn->control_points;

    /*
     * This part must be edited. Repeat this for every per connection control point that starts
     * enabled.
     */
    /* From examples/ex_control_points.c */
    WT_RET(__wti_conn_control_point_enable(
      session, &(control_points[WT_CONN_CONTROL_POINT_ID_MainStartPrinting]), cfg));
#endif
    return (0);
}

/*
 * __wt_session_control_point_enable_all --
 *     Enable per session control points that start enabled. Note, one part of this function must be
 *     edited for each per session control point that starts enabled.
 *
 * @param session The session.
 */
int
__wt_session_control_point_enable_all(WT_SESSION_IMPL *session)
{
#if 1 /* If no per session control points are enabled at the start. */
    WT_UNUSED(session);
#else
    WT_CONTROL_POINT_REGISTRY *control_points;
    if (SESSION_CONTROL_POINTS_SIZE == 0)
        return (0);

    /* Lazy initialization. */
    control_points = session->control_points;
    if (control_points == NULL) {
        WT_RET(__wt_session_control_point_init_all(session));
        control_points = session->control_points;
    }

    /*
     * This part must be edited. Repeat this for every per session control point that starts
     * enabled.
     */
#if 0 /* For example. */
    WT_RET(__wti_session_control_point_enable(session,
        &(control_points[WT_SESSION_CONTROL_POINT_ID_EXAMPLE2])));
#endif
#endif
    return (0);
}
#endif /* HAVE_CONTROL_POINTS */
