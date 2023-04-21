// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see 
// http://www.gnu.org/licenses/gpl-2.0.html.

#include "driver.h"

#include <chrono>
#include <future>
#include <thread>

// **** FAILOVER_SYNC ***************************************
// used for thread synchronization
FAILOVER_SYNC::FAILOVER_SYNC(int num_tasks) : num_tasks{num_tasks} {}

void FAILOVER_SYNC::increment_task() {
    std::lock_guard<std::mutex> lock(mutex_);
    num_tasks++;
}

void FAILOVER_SYNC::mark_as_complete(bool cancel_other_tasks) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cancel_other_tasks) {
        num_tasks = 0;
    } else {
        if (num_tasks <= 0) {
            throw std::runtime_error("Trying to cancel a failover process that is already done.");
        }
        num_tasks--;
    }

    cv.notify_one();
}

void FAILOVER_SYNC::wait_and_complete(int milliseconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return num_tasks <= 0; });
    num_tasks = 0;
}

bool FAILOVER_SYNC::is_completed() {
    std::unique_lock<std::mutex> lock(mutex_);
    return num_tasks <= 0; 
}

// ************* FAILOVER ***********************************
// Base class of two writer failover task handlers
FAILOVER::FAILOVER(
    std::shared_ptr<CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    unsigned long dbc_id, bool enable_logging)
    : connection_handler{connection_handler},
      topology_service{topology_service},
      dbc_id{dbc_id},
      new_connection{nullptr} {

    if (enable_logging)
        logger = init_log_file();
}

bool FAILOVER::is_writer_connected() {
    return new_connection && new_connection->is_connected();
}

bool FAILOVER::connect(std::shared_ptr<HOST_INFO> host_info) {
    new_connection = connection_handler->connect(host_info, nullptr);
    return is_writer_connected();
}

void FAILOVER::sleep(int miliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds));
}

// Close new connection if not needed (other task finishes and returns first)
void FAILOVER::release_new_connection() {
    if (new_connection) {
        new_connection->delete_ds();
        delete new_connection;
        new_connection = nullptr;
    }
}

// ************************ RECONNECT_TO_WRITER_HANDLER
// handler reconnecting to a given host, e.g. reconnect to a current writer
RECONNECT_TO_WRITER_HANDLER::RECONNECT_TO_WRITER_HANDLER(
    std::shared_ptr<CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    int connection_interval, unsigned long dbc_id, bool enable_logging)
    : FAILOVER{connection_handler, topology_service, dbc_id, enable_logging},
      reconnect_interval_ms{connection_interval} {}

RECONNECT_TO_WRITER_HANDLER::~RECONNECT_TO_WRITER_HANDLER() {}

void RECONNECT_TO_WRITER_HANDLER::operator()(
    int id,
    std::shared_ptr<HOST_INFO> original_writer,
    std::shared_ptr<FAILOVER_SYNC> f_sync,
    std::shared_ptr<WRITER_FAILOVER_RESULT> result) {

    if (original_writer) {
        MYLOG_TRACE(logger, dbc_id,
                    "Thread ID %d - [RECONNECT_TO_WRITER_HANDLER] [TaskA] Attempting to "
                    "re-connect to the current writer instance: %s",
                    id, original_writer->get_host_port_pair().c_str());

        while (!f_sync->is_completed()) {
            if (connect(original_writer)) {
                auto latest_topology =
                    topology_service->get_topology(new_connection, true);
                if (latest_topology->total_hosts() > 0 &&
                    is_current_host_writer(original_writer, latest_topology)) {

                    topology_service->mark_host_up(original_writer);
                    if (f_sync->is_completed()) {
                        break;
                    }
                    result->connected = true;
                    result->is_new_host = false;
                    result->new_topology = latest_topology;
                    result->new_connection = std::move(new_connection);
                    f_sync->mark_as_complete(true);
                    MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [RECONNECT_TO_WRITER_HANDLER] [TaskA] Finished", id);
                    return;
                }
                release_new_connection();
            }
            sleep(reconnect_interval_ms);
        }
        MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [RECONNECT_TO_WRITER_HANDLER] [TaskA] Cancelled", id);
    }
    // Another thread finishes or both timeout, this thread is canceled
    release_new_connection();
    MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [RECONNECT_TO_WRITER_HANDLER] [TaskA] Finished", id);
}

bool RECONNECT_TO_WRITER_HANDLER::is_current_host_writer(
    std::shared_ptr<HOST_INFO> original_writer,
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> latest_topology) {
    auto original_instance = original_writer->instance_name;
    if (original_instance.empty()) return false;
    auto latest_writer = latest_topology->get_writer();
    auto latest_instance = latest_writer->instance_name;
    return latest_instance == original_instance;
}

// ************************ WAIT_NEW_WRITER_HANDLER
// handler getting the latest cluster topology and connecting to a newly elected
// writer
WAIT_NEW_WRITER_HANDLER::WAIT_NEW_WRITER_HANDLER(
    std::shared_ptr<CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology,
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
    int connection_interval, unsigned long dbc_id, bool enable_logging)
    : FAILOVER{connection_handler, topology_service, dbc_id, enable_logging},
      current_topology{current_topology},
      reader_handler{reader_handler},
      read_topology_interval_ms{connection_interval} {}

WAIT_NEW_WRITER_HANDLER::~WAIT_NEW_WRITER_HANDLER() {}

void WAIT_NEW_WRITER_HANDLER::operator()(
    int id,
    std::shared_ptr<HOST_INFO> original_writer,
    std::shared_ptr<FAILOVER_SYNC> f_sync,
    std::shared_ptr<WRITER_FAILOVER_RESULT> result) {

    MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [WAIT_NEW_WRITER_HANDLER] [TaskB] Attempting to connect to a new writer instance", id);
    
    while (!f_sync->is_completed()) {
        if (!is_writer_connected()) {
            connect_to_reader(f_sync);
            refresh_topology_and_connect_to_new_writer(original_writer, f_sync);
            clean_up_reader_connection();
        } else {
            result->connected = true;
            result->is_new_host = true;
            result->new_topology = current_topology;
            result->new_connection = std::move(new_connection);
            f_sync->mark_as_complete(true);
            MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [WAIT_NEW_WRITER_HANDLER] [TaskB] Finished", id);
            return;
        }
    }
    MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [WAIT_NEW_WRITER_HANDLER] [TaskB] Cancelled", id);

    // Another thread finishes or both timeout, this thread is canceled
    clean_up_reader_connection();
    release_new_connection();
    MYLOG_TRACE(logger, dbc_id, "Thread ID %d - [WAIT_NEW_WRITER_HANDLER] [TaskB] Finished", id);
}

// Connect to a reader to later retrieve the latest topology
void WAIT_NEW_WRITER_HANDLER::connect_to_reader(std::shared_ptr<FAILOVER_SYNC> f_sync) {
    while (!f_sync->is_completed()) {
        auto connection_result = reader_handler->get_reader_connection(current_topology, f_sync);
        if (connection_result->connected && connection_result->new_connection->is_connected()) {
            reader_connection = connection_result->new_connection;
            current_reader_host = connection_result->new_host;
            MYLOG_TRACE(
                logger, dbc_id,
                "[WAIT_NEW_WRITER_HANDLER] [TaskB] Connected to reader: %s",
                connection_result->new_host->get_host_port_pair().c_str());
            break;
        }
        MYLOG_TRACE(logger, dbc_id, "[WAIT_NEW_WRITER_HANDLER] [TaskB] Failed to connect to any reader.");
    }
}

// Use just connected reader to refresh topology and try to connect to a new writer
void WAIT_NEW_WRITER_HANDLER::refresh_topology_and_connect_to_new_writer(
    std::shared_ptr<HOST_INFO> original_writer, std::shared_ptr<FAILOVER_SYNC> f_sync) {
    while (!f_sync->is_completed()) {
        auto latest_topology = topology_service->get_topology(reader_connection, true);
        if (latest_topology->total_hosts() > 0) {
            current_topology = latest_topology;
            auto writer_candidate = current_topology->get_writer();
            // Same case is handled by the reconnect handler
            if (!HOST_INFO::is_host_same(writer_candidate, original_writer)) {
                if (connect_to_writer(writer_candidate)) return;
            }
        }
        sleep(read_topology_interval_ms);
    }
}

// Try to connect to the writer candidate
bool WAIT_NEW_WRITER_HANDLER::connect_to_writer(
    std::shared_ptr<HOST_INFO> writer_candidate) {

    MYLOG_TRACE(logger, dbc_id,
      "[WAIT_NEW_WRITER_HANDLER] [TaskB] Trying to connect to a new writer: %s",
      writer_candidate->get_host_port_pair().c_str());

    if (HOST_INFO::is_host_same(writer_candidate, current_reader_host)) {
        new_connection = reader_connection;
    } else if (!connect(writer_candidate)) {
        topology_service->mark_host_down(writer_candidate);
        return false;
    }
    topology_service->mark_host_up(writer_candidate);
    return true;
}

// Close reader connection if not needed (open and not the same as current connection)
void WAIT_NEW_WRITER_HANDLER::clean_up_reader_connection() {
    if (reader_connection && new_connection != reader_connection) {
        reader_connection->delete_ds();
        delete reader_connection;
        reader_connection = nullptr;
    }
}

// ************************** FAILOVER_WRITER_HANDLER  **************************

FAILOVER_WRITER_HANDLER::FAILOVER_WRITER_HANDLER(
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
    std::shared_ptr<CONNECTION_HANDLER> connection_handler,
    ctpl::thread_pool& thread_pool,
    int writer_failover_timeout_ms, int read_topology_interval_ms,
    int reconnect_writer_interval_ms, unsigned long dbc_id, bool enable_logging)
    : connection_handler{connection_handler},
      topology_service{topology_service},
      reader_handler{reader_handler},
      thread_pool{thread_pool},
      writer_failover_timeout_ms{writer_failover_timeout_ms},
      read_topology_interval_ms{read_topology_interval_ms},
      reconnect_writer_interval_ms{reconnect_writer_interval_ms},
      dbc_id{dbc_id} {

    if (enable_logging)
        logger = init_log_file();
}

FAILOVER_WRITER_HANDLER::~FAILOVER_WRITER_HANDLER() {}

std::shared_ptr<WRITER_FAILOVER_RESULT> FAILOVER_WRITER_HANDLER::failover(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology) {
    
    if (!current_topology || current_topology->total_hosts() == 0) {
        MYLOG_TRACE(logger, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Failover was called with "
                    "an invalid (null or empty) topology");
        return std::make_shared<WRITER_FAILOVER_RESULT>(false, false, nullptr, nullptr);
    }

    const auto start = std::chrono::steady_clock::now();

    auto failover_sync = std::make_shared<FAILOVER_SYNC>(2);

    // Constructing the function objects
    RECONNECT_TO_WRITER_HANDLER reconnect_handler(
        connection_handler, topology_service, reconnect_writer_interval_ms, dbc_id, logger != nullptr);
    WAIT_NEW_WRITER_HANDLER new_writer_handler(
        connection_handler, topology_service, current_topology, reader_handler,
        read_topology_interval_ms, dbc_id, logger != nullptr);

    auto original_writer = current_topology->get_writer();
    topology_service->mark_host_down(original_writer);

    auto reconnect_result = std::make_shared<WRITER_FAILOVER_RESULT>(false, false, nullptr, nullptr);
    auto new_writer_result = std::make_shared<WRITER_FAILOVER_RESULT>(false, false, nullptr, nullptr);

    if (thread_pool.n_idle() <= 1) {
        int size = thread_pool.size() + 2 - thread_pool.n_idle();
        MYLOG_TRACE(logger, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Resizing thread pool to %d", size);
        thread_pool.resize(size);
    }

    auto reconnect_future = thread_pool.push(std::move(reconnect_handler), original_writer, failover_sync, reconnect_result);
    auto wait_new_writer_future = thread_pool.push(std::move(new_writer_handler), original_writer, failover_sync, new_writer_result);

    // Wait for task complete signal with specified timeout
    failover_sync->wait_and_complete(writer_failover_timeout_ms);

    // Constantly polling for results until timeout
    while (true) {
        // Check if reconnect task result is ready
        if (reconnect_future.valid() && reconnect_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            reconnect_future.get();
            if (reconnect_result->connected) {
                MYLOG_TRACE(logger, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Successfully re-connected to the current writer instance: %s",
                    reconnect_result->new_topology->get_writer()->get_host_port_pair().c_str());
                return reconnect_result;
            }
        }

        // Check if wait new writer task result is ready
        if (wait_new_writer_future.valid() && wait_new_writer_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            wait_new_writer_future.get();
            if (new_writer_result->connected) {
                MYLOG_TRACE(logger, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Successfully connected to the new writer instance: %s",
                    new_writer_result->new_topology->get_writer()->get_host_port_pair().c_str());
                return new_writer_result;
            }
        }

        // Results are ready but non has valid connection
        if (!reconnect_future.valid() && !wait_new_writer_future.valid()) {
            break;
        }

        // No result it ready, update remaining timeout
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        const auto remaining_wait_ms = writer_failover_timeout_ms - duration.count();
        if (remaining_wait_ms <= 0) {
            // Writer failover timed out
            MYLOG_TRACE(logger, dbc_id, "[FAILOVER_WRITER_HANDLER] Writer failover timed out. Failed to connect to the writer instance.");
            return std::make_shared<WRITER_FAILOVER_RESULT>(false, false, nullptr, nullptr);
        }
    }

    // Writer failover finished but not connected
    MYLOG_TRACE(logger, dbc_id, "[FAILOVER_WRITER_HANDLER] Failed to connect to the writer instance.");
    return std::make_shared<WRITER_FAILOVER_RESULT>(false, false, nullptr, nullptr);
}
