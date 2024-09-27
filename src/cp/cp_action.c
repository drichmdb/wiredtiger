/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#ifdef HAVE_CONTROL_POINTS

/* cp_action.c: Definitions for control point actions. */
/* This file must be edited when a new control point action is created. */

/*
 * Action: Sleep: Delay at a specific code location during an execution via __wt_sleep.
 */
/* Action config parsing function. */
/*
 * __wt_control_point_config_action_sleep --
 *     Configuration parsing for control point action "Sleep: Delay at a specific code location
 *     during an execution".
 *
 * @param session The session. @param data Return the parsed data in here. @param cfg The
 *     configuration strings.
 */
int
__wt_control_point_config_action_sleep(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT *data, const char **cfg)
{
    /* TODO. Replace these hard wired values with control point action configuration parsing. */
    /* TODO. When the hard wire is removed, delete this function from func_ok() in dist/s_void. */
    WT_CONTROL_POINT_ACTION_SLEEP *action_data = (WT_CONTROL_POINT_ACTION_SLEEP *)(data + 1);
    WT_UNUSED(session);
    WT_UNUSED(cfg);
    action_data->seconds = 2;
    action_data->microseconds = 3;
    return (0);
}

/*
 * Action: ERR: Change the control flow to trigger an error condition via WT_ERR.
 */
/* Action config parsing function. */
/*
 * __wt_control_point_config_action_err --
 *     Configuration parsing for control point action "ERR: Change the control flow to trigger an
 *     error condition".
 *
 * @param session The session. @param data Return the parsed data in here. @param cfg The
 *     configuration strings.
 */
int
__wt_control_point_config_action_err(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT *data, const char **cfg)
{
    /* TODO. Replace these hard wired values with control point action configuration parsing. */
    /* TODO. When the hard wire is removed, delete this function from func_ok() in dist/s_void. */
    WT_CONTROL_POINT_ACTION_ERR *action_data = (WT_CONTROL_POINT_ACTION_ERR *)(data + 1);
    WT_UNUSED(session);
    WT_UNUSED(cfg);
    action_data->err = WT_ERROR;
    return (0);
}

/*
 * Action: RET: Return an error via WT_RET.
 */
/* Action config parsing function. */
/*
 * __wt_control_point_config_action_ret --
 *     Configuration parsing for control point action "RET: Return an error".
 *
 * @param session The session. @param data Return the parsed data in here. @param cfg The
 *     configuration strings.
 */
int
__wt_control_point_config_action_ret(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT *data, const char **cfg)
{
    /* TODO. Replace these hard wired values with control point action configuration parsing. */
    /* TODO. When the hard wire is removed, delete this function from func_ok() in dist/s_void. */
    WT_CONTROL_POINT_ACTION_RET *action_data = (WT_CONTROL_POINT_ACTION_RET *)(data + 1);
    WT_UNUSED(session);
    WT_UNUSED(cfg);
    action_data->ret_value = WT_ERROR;
    return (0);
}

/*
 * Action: Wait for trigger: Blocking the testing thread until a control point is triggered.
 */
#define WT_DELAY_UNTIL_TRIGGERED_USEC (10 * WT_THOUSAND) /* 10 milliseconds */

/* Action config parsing function. */
/*
 * __wt_control_point_config_action_wait_for_trigger --
 *     Configuration parsing for control point action "Wait until trigger: Blocking the testing
 *     thread until a control point is triggered".
 *
 * @param session The session. @param data Return the parsed data in here. @param cfg The
 *     configuration strings.
 */
int
__wt_control_point_config_action_wait_for_trigger(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT *data, const char **cfg)
{
    /* TODO. Replace these hard wired values with control point action configuration parsing. */
    /* TODO. When the hard wire is removed, delete this function from func_ok() in dist/s_void. */
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *action_data =
      (WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *)(data + 1);
    WT_UNUSED(session);
    WT_UNUSED(cfg);
    action_data->wait_count = 1;
    return (0);
}

/* Functions used at the call site. */
/*
 * __wt_control_point_run_wait_for_trigger --
 *     The run function for __wt_cond_wait_signal for the call site portion of control point action
 *     "Wait until trigger: Blocking the testing thread until a control point is triggered".
 *
 * @param session The session.
 */
bool
__wt_control_point_run_wait_for_trigger(WT_SESSION_IMPL *session)
{
    WT_CONTROL_POINT_REGISTRY *cp_registry = session->cp_registry;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *action_data =
      (WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *)(session->cp_data + 1);
    return (cp_registry->trigger_count >= action_data->desired_trigger_count);
}

/*
 * __wt_control_point_wait_for_trigger --
 *     The call site portion of control point action "Wait until trigger: Blocking the testing
 *     thread until a control point is triggered" given a WT_CONTROL_POINT_REGISTRY. Return true if
 *     triggered.
 *
 * @param session The session. @param cp_registry The control point's registry.
 */
bool
__wt_control_point_wait_for_trigger(
  WT_SESSION_IMPL *session, WT_CONTROL_POINT_REGISTRY *cp_registry)
{
    size_t start_trigger_count = cp_registry->trigger_count;
    size_t desired_trigger_count;
    WT_CONTROL_POINT *data = __wt_control_point_get_data(session, cp_registry, true);
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *action_data;
    bool signalled;
    if (WT_UNLIKELY(data == NULL))
        return (false); /* Not enabled. */
    /* Is waiting necessary? */
    action_data = (WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *)(data + 1);
    desired_trigger_count = start_trigger_count + action_data->wait_count;
    if (cp_registry->trigger_count >= desired_trigger_count) { /* No */
        __wt_control_point_release_data(session, cp_registry, data, true);
        return (true); /* Enabled and wait fulfilled. */
    }
    /* Store data needed by run_func. */
    action_data->desired_trigger_count = desired_trigger_count;
    session->cp_registry = cp_registry;
    session->cp_data = data;
    __wt_control_point_unlock(session, cp_registry);
    for (;;) {
        __wt_cond_wait_signal(session, action_data->condvar, WT_DELAY_UNTIL_TRIGGERED_USEC,
          __wt_control_point_run_wait_for_trigger, &signalled);
        if (cp_registry->trigger_count >= desired_trigger_count)
            /* Delay condition satisfied. */
            break;
    }
    __wt_control_point_release_data(session, cp_registry, data, false);
    return (true);
}

/* Extra initialization. */
/*
 * __wt_control_point_action_init_wait_for_trigger --
 *     Extra initialization required for action "Wait until trigger: Blocking the testing thread
 *     until a control point is triggered".
 *
 * @param session The session. @param control_point_name The name of the control point. @param data
 *     The control point's data.
 */
void
__wt_control_point_action_init_wait_for_trigger(
  WT_SESSION_IMPL *session, const char *control_point_name, WT_CONTROL_POINT *data)
{
    WT_DECL_RET;
    WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *action_data =
      (WT_CONTROL_POINT_ACTION_WAIT_FOR_TRIGGER *)(data + 1);
    ret = __wt_cond_alloc(session, control_point_name, &(action_data->condvar));
    WT_ASSERT(session, ret == 0);
}

#endif /* HAVE_CONTROL_POINTS */
