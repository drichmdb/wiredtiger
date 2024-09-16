/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *      All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include <utility>

#include "mock_session.h"
#include "mock_connection.h"
#include "../utils.h"

mock_session::mock_session(
  WT_SESSION_IMPL *session, std::shared_ptr<mock_connection> mock_connection)
    : _session_impl(session), _mock_connection(std::move(mock_connection))
{
    // We could initialize this in the list but order is important and this seems easier.
    _handler_wrap.handler = {
      handle_wiredtiger_error, handle_wiredtiger_message, nullptr, nullptr, nullptr};
    _handler_wrap.ms = this;

    _session_impl->event_handler = &_handler_wrap.handler;
}

mock_session::~mock_session()
{
    WT_CONNECTION_IMPL *connection_impl = _mock_connection->get_wt_connection_impl();
    if (_session_impl->block_manager != nullptr)
        _session_impl->block_manager_cleanup(_session_impl);
    // FIXME-WT-13505: Move terminate function to connection once circular dependency is fixed.
    if (connection_impl->file_system != nullptr)
        utils::throw_if_non_zero(connection_impl->file_system->terminate(
          connection_impl->file_system, reinterpret_cast<WT_SESSION *>(_session_impl)));
    if (_session_impl->dhandle != nullptr) {
        if (_session_impl->dhandle->handle != nullptr)
                    __wt_free(nullptr, _session_impl->dhandle->handle);
        __wt_free(nullptr, _session_impl->dhandle);
    }
    __wt_free(nullptr, _session_impl);
}

std::shared_ptr<mock_session>
mock_session::build_test_mock_session()
{
    auto mock_connection = mock_connection::build_test_mock_connection();

    WT_SESSION_IMPL *session_impl = nullptr;
    utils::throw_if_non_zero(__wt_calloc(nullptr, 1, sizeof(WT_SESSION_IMPL), &session_impl));
    session_impl->iface.connection = mock_connection->get_wt_connection();

    // Construct an object that will now own the two pointers passed in.
    return std::shared_ptr<mock_session>(new mock_session(session_impl, mock_connection));
}

WT_BLOCK_MGR_SESSION *
mock_session::setup_block_manager_session()
{
    // Initialize rnd state because block manager requires it.
    __wt_random_init(&_session_impl->rnd);
    utils::throw_if_non_zero(
      __wt_calloc(nullptr, 1, sizeof(WT_BLOCK_MGR_SESSION), &_session_impl->block_manager));

    return static_cast<WT_BLOCK_MGR_SESSION *>(_session_impl->block_manager);
}

void
mock_session::setup_block_manager_file_operations()
{
    // Initialize the checksum function.
    __wt_process.checksum = wiredtiger_crc32c_func();
    utils::throw_if_non_zero(
      __wt_calloc(nullptr, 1, sizeof(WT_DATA_HANDLE), &_session_impl->dhandle));
    utils::throw_if_non_zero(
      __wt_calloc(nullptr, 1, sizeof(WT_BTREE), &_session_impl->dhandle->handle));
}

int
handle_wiredtiger_error(WT_EVENT_HANDLER *handler, WT_SESSION *session, int, const char *message)
{
    handle_wiredtiger_message(handler, session, message);
    return (0);
}

int
handle_wiredtiger_message(WT_EVENT_HANDLER *handler, WT_SESSION *, const char *message)
{
    reinterpret_cast<event_handler_wrap *>(handler)->ms->add_callback_message(message);
    return (0);
}
