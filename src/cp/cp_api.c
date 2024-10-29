/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/* cp_api.c: Definitions for the control point API. */

#include "wt_internal.h"

#ifdef HAVE_CONTROL_POINT
/*
 * Lock/unlock functions used by per-connection control points.
 */
/*
 * __wti_control_point_get_data --
 *     Get cp_registry->cp_data safe from frees.
 *
 * Return unlocked if !locked or data == NULL. Otherwise, locked and data != NULL, return locked.
 *
 * @param session The session. @param cp_registry The control point registry. @param locked True if
 *     cp_registry->lock is left locked for additional processing along with incrementing the
 *     ref_count.
 */
WT_CONTROL_POINT_DATA *
__wti_control_point_get_data(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT_REGISTRY *cp_registry, bool locked)
{
    WT_CONTROL_POINT_DATA *saved_cp_data;

    WT_ASSERT(session, !__wt_spin_owned(session, &cp_registry->lock));
    __wt_spin_lock(session, &cp_registry->lock);

    saved_cp_data = cp_registry->cp_data;
    if (saved_cp_data != NULL)
        __wt_atomic_add32(&saved_cp_data->ref_count, 1);

    if (!locked || (saved_cp_data == NULL))
        __wt_spin_unlock(session, &cp_registry->lock);

    return (saved_cp_data);
}

/*
 * __wt_control_point_unlock --
 *     Unlock after additional processing.
 *
 * This is called after finishing the additional processing started with
 *     __wti_control_point_get_data() with locked=true.
 *
 * @param session The session. @param cp_registry The control point registry.
 */
void
__wt_control_point_unlock(WT_SESSION_IMPL *session, WT_CONTROL_POINT_REGISTRY *cp_registry)
{
    WT_ASSERT(session, __wt_spin_owned(session, &cp_registry->lock));
    __wt_spin_unlock(session, &cp_registry->lock);
}

/*
 * __wti_control_point_relock --
 *     Lock cp_registry->lock again after unlocking.
 *
 * This relocks after __wti_control_point_get_data() and __wt_control_point_unlock().
 *
 * @param session The session. @param cp_registry The control point registry. @param cp_data The
 *     control point data last time.
 */
void
__wti_control_point_relock(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT_REGISTRY *cp_registry, WT_CONTROL_POINT_DATA *cp_data)
{
    WT_ASSERT(session, !__wt_spin_owned(session, &cp_registry->lock));
    __wt_spin_lock(session, &cp_registry->lock);
    WT_ASSERT(session, cp_registry->cp_data == cp_data);
}

/*
 * __wt_control_point_release_data --
 *     Call when done using WT_CONTROL_POINT_REGISTRY->cp_data that was returned by
 *     __wti_control_point_get_data.
 *
 * Unlocked at return.
 *
 * @param session The session. @param cp_registry The control point registry. @param locked True if
 *     the control point data is already locked.
 */
void
__wt_control_point_release_data(WT_SESSION_IMPL *session, WT_CONTROL_POINT_REGISTRY *cp_registry,
  WT_CONTROL_POINT_DATA *cp_data, bool locked)
{
    uint32_t new_ref;

    if (locked) {
        WT_ASSERT(session, __wt_spin_owned(session, &cp_registry->lock));
    } else {
        WT_ASSERT(session, !__wt_spin_owned(session, &cp_registry->lock));
    }

    if (WT_UNLIKELY(cp_data == NULL)) {
        if (locked)
            __wt_spin_unlock(session, &cp_registry->lock);
        return;
    }

    if (!locked)
        __wt_spin_lock(session, &cp_registry->lock);

    new_ref = __wt_atomic_sub32(&cp_registry->cp_data->ref_count, 1);
    if ((new_ref == 0) && (cp_registry->cp_data != cp_data))
        __wt_free(session, cp_data);

    __wt_spin_unlock(session, &cp_registry->lock);
}

/*
 * Get functions used to implement the API.
 */
/*
 * __wti_conn_control_point_get_registry --
 *     Get the control point registry of a per-connection control point.
 */
int
__wti_conn_control_point_get_registry(
  WT_CONNECTION_IMPL *conn, wt_control_point_id_t id, WT_CONTROL_POINT_REGISTRY **cp_registryp)
{
    WT_DECL_RET;
#if CONNECTION_CONTROL_POINTS_SIZE == 0
    WT_ERR(EINVAL);
    WT_UNUSED(conn);
    WT_UNUSED(id);
    WT_UNUSED(cp_registryp);
#else
    if (WT_UNLIKELY(id >= CONNECTION_CONTROL_POINTS_SIZE))
        WT_ERR(EINVAL);
    if (WT_UNLIKELY(F_ISSET(conn, WT_CONN_SHUTTING_DOWN)))
        WT_ERR(EINVAL);
    if (conn->control_points == NULL)
        WT_ERR(WT_CP_DISABLED);
    *cp_registryp = &(conn->control_points[id]);
#endif
err:
    return (ret);
}

/*
 * __conn_control_point_get_data --
 *     Get the control point data of a per-connection control point.
 */
static int
__conn_control_point_get_data(
  WT_CONNECTION_IMPL *conn, wt_control_point_id_t id, WT_CONTROL_POINT_DATA **cp_datap)
{
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_DECL_RET;

    WT_ERR(__wti_conn_control_point_get_registry(conn, id, &cp_registry));
    *cp_datap = cp_registry->cp_data;
err:
    return (ret);
}

/*
 * API: Get from WT_CONTROL_POINT_REGISTRY.
 */
/*
 * __wt_conn_control_point_get_crossing_count --
 *     Get the crossing count of a per-connection control point.
 */
int
__wt_conn_control_point_get_crossing_count(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, size_t *crossing_countp)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__wti_conn_control_point_get_registry(conn, id, &cp_registry));
    *crossing_countp = cp_registry->crossing_count;
err:
    return (ret);
}

/*
 * __wt_conn_control_point_get_trigger_count --
 *     Get the trigger count of a per-connection control point.
 */
int
__wt_conn_control_point_get_trigger_count(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, size_t *trigger_countp)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__wti_conn_control_point_get_registry(conn, id, &cp_registry));
    *trigger_countp = cp_registry->trigger_count;
err:
    return (ret);
}

/*
 * API: Get from WT_CONTROL_POINT_DATA and set in WT_CONTROL_POINT_DATA.
 */
/*
 * __wt_conn_control_point_is_enabled --
 *     Get whether a per-connection control point is enabled.
 */
int
__wt_conn_control_point_is_enabled(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, bool *is_enabledp)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_DATA *cp_data;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__conn_control_point_get_data(conn, id, &cp_data));
    *is_enabledp = (cp_data != NULL);
err:
    return (ret);
}

/*
 * __wt_conn_control_point_get_param1 --
 *     Get param1 of a per-connection control point with predicate "Param 64 match".
 */
int
__wt_conn_control_point_get_param1(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, uint64_t *value64p)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_DATA *cp_data;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__conn_control_point_get_data(conn, id, &cp_data));
    *value64p = cp_data->param1.value64;
err:
    return (ret);
}

/*
 * __wt_conn_control_point_get_param2 --
 *     Get param2 of a per-connection control point with predicate "Param 64 match".
 */
int
__wt_conn_control_point_get_param2(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, uint64_t *value64p)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_DATA *cp_data;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__conn_control_point_get_data(conn, id, &cp_data));
    *value64p = cp_data->param2.value64;
err:
    return (ret);
}

/*
 * __wt_conn_control_point_set_param1 --
 *     Set param1 of a per-connection control point with predicate "Param 64 match".
 *
 * Note, this is only for use with predicate "Param 64 match". The configuration strings are not
 *     changed. If WT_CONNECTION.disable_control_point() and WT_CONNECTION.enable_control_point()
 *     are called the change is lost.
 */
int
__wt_conn_control_point_set_param1(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, uint64_t value64)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_DATA *cp_data;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__conn_control_point_get_data(conn, id, &cp_data));
    cp_data->param1.value64 = value64;
err:
    return (ret);
}

/*
 * __wt_conn_control_point_set_param2 --
 *     Set param2 of a per-connection control point with predicate "Param 64 match".
 *
 * Note, this is only for use with predicate "Param 64 match". The configuration strings are not
 *     changed. If WT_CONNECTION.disable_control_point() and WT_CONNECTION.enable_control_point()
 *     are called the change is lost.
 */
int
__wt_conn_control_point_set_param2(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, uint64_t value64)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_DATA *cp_data;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__conn_control_point_get_data(conn, id, &cp_data));
    cp_data->param2.value64 = value64;
err:
    return (ret);
}

/*
 * API: Disable a per connection control point.
 */
/*
 * __conn_control_point_disable --
 *     Disable a per connection control point given a WT_CONTROL_POINT_REGISTRY.
 *
 * @param conn The connection. @param cp_registry The WT_CONTROL_POINT_REGISTRY of the per
 *     connection control point to disable.
 */
static int
__conn_control_point_disable(WT_CONNECTION_IMPL *conn, WT_CONTROL_POINT_REGISTRY *cp_registry)
{
    WT_CONTROL_POINT_DATA *saved_cp_data;
    WT_DECL_RET;
    WT_UNUSED(conn);

    __wt_spin_lock(NULL, &cp_registry->lock);
    saved_cp_data = cp_registry->cp_data;
    if (WT_UNLIKELY(saved_cp_data == NULL))
        /* Already disabled. */
        WT_ERR(WT_NOTFOUND);
    cp_registry->cp_data = NULL;
    if (__wt_atomic_loadv32(&saved_cp_data->ref_count) == 0)
        __wt_free(NULL, saved_cp_data);
#if 0 /* TODO. */
    else
        /* TODO: Add saved_cp_data to a queue of control point data waiting to be freed. */;
#endif
err:
    __wt_spin_unlock(NULL, &cp_registry->lock);
    return (ret);
}

/*
 * __wt_conn_control_point_disable --
 *     Disable a per connection control point.
 *
 * @param wt_conn The connection. @param id The ID of the per connection control point to disable.
 */
int
__wt_conn_control_point_disable(WT_CONNECTION *wt_conn, wt_control_point_id_t id)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__wti_conn_control_point_get_registry(conn, id, &cp_registry));
    ret = __conn_control_point_disable(conn, cp_registry);
err:
    return (ret);
}

/*
 * API: Enable a per connection control point.
 */
/*
 * __wti_conn_control_point_enable --
 *     Enable a per connection control point given a WT_CONTROL_POINT_REGISTRY.
 *
 * @param conn The connection. @param cp_registry The registry of the per connection control point
 *     to enable. @param cfg The configuration strings.
 */
int
__wti_conn_control_point_enable(
  WT_CONNECTION_IMPL *conn, WT_CONTROL_POINT_REGISTRY *cp_registry, const char **cfg)
{
    WT_CONTROL_POINT_DATA *cp_data;
    WT_DECL_RET;
    WT_UNUSED(conn);

    __wt_spin_lock(NULL, &cp_registry->lock);
    cp_data = cp_registry->cp_data;
    if (WT_UNLIKELY(cp_data != NULL))
        /* Already enabled. */
        WT_ERR(EEXIST);
    WT_ERR(
      cp_registry->init(NULL, cp_registry->config_name, cp_registry->init_pred, cfg, &cp_data));
    cp_registry->cp_data = cp_data;
err:
    __wt_spin_unlock(NULL, &cp_registry->lock);
    return (ret);
}

/*
 * __wt_conn_control_point_enable --
 *     Enable a per connection control point.
 *
 * @param wt_conn The connection. @param id The ID of the per connection control point to enable.
 *     @param cfg The configuration string override.
 */
int
__wt_conn_control_point_enable(
  WT_CONNECTION *wt_conn, wt_control_point_id_t id, const char *extra_cfg)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    const char *cfg[3];

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    cp_registry = &(conn->control_points[id]);
    cfg[0] = conn->cfg;
    cfg[1] = extra_cfg;
    cfg[2] = NULL;
    return (__wti_conn_control_point_enable(conn, cp_registry, cfg));
}

/*
 * __wt_conn_control_point_shutdown --
 *     Shut down the per connection control points.
 *
 * @param conn The connection.
 */
int
__wt_conn_control_point_shutdown(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *control_points;
    WT_DECL_RET;
    int one_ret;

    conn = S2C(session);
    control_points = conn->control_points;
    if (control_points == NULL)
        return (0);
    /* Stop new per connection control point operations. */
    F_SET(conn, WT_CONN_SHUTTING_DOWN);

    for (int idx = 0; idx < CONNECTION_CONTROL_POINTS_SIZE; ++idx) {
        if (control_points[idx].cp_data == NULL)
            continue;
        one_ret = __conn_control_point_disable(conn, &(control_points[idx]));
        if (one_ret != 0)
            ret = one_ret; /* Return the last error. */
    }
    /* TODO: Wait for all disable operations to finish. */
    return (ret);
}

/*
 * __wt_conn_control_point_thread_barrier --
 *     Wait for a control point with action "Thread Barrier".
 *
 * This function is equivalent to macro CONNECTION_CONTROL_POINT_DEFINE_THREAD_BARRIER. Making the
 *     macro into a function allows it to be called from python.
 *
 * @param wt_conn The connection. @param id The ID of the per connection control point to disable.
 */
int
__wt_conn_control_point_thread_barrier(WT_CONNECTION *wt_conn, wt_control_point_id_t id)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONTROL_POINT_REGISTRY *cp_registry;
    WT_DECL_RET;

    conn = (WT_CONNECTION_IMPL *)wt_conn;
    WT_ERR(__wti_conn_control_point_get_registry(conn, id, &cp_registry));
    CONNECTION_CONTROL_POINT_DEFINE_THREAD_BARRIER(conn->default_session, id);
err:
    return (ret);
}

#endif /* HAVE_CONTROL_POINT */
