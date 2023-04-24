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

#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <thread>
#include <vector>

FAILOVER_READER_HANDLER::FAILOVER_READER_HANDLER(
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    std::shared_ptr<CONNECTION_HANDLER> connection_handler,
    ctpl::thread_pool& thread_pool,
    int failover_timeout_ms, int failover_reader_connect_timeout,
    bool enable_strict_reader_failover,
    unsigned long dbc_id, bool enable_logging)
    : topology_service{topology_service},
      connection_handler{connection_handler},
      thread_pool{thread_pool},
      max_failover_timeout_ms{failover_timeout_ms},
      reader_connect_timeout_ms{failover_reader_connect_timeout},
      enable_strict_reader_failover{enable_strict_reader_failover},
      dbc_id{dbc_id} {

    if (enable_logging)
        logger = init_log_file();
}

FAILOVER_READER_HANDLER::~FAILOVER_READER_HANDLER() {}

// Function called to start the Reader Failover process.
// This process will generate a list of available hosts: First readers that are up, then readers marked as down, then writers.
// If it goes through the list and does not succeed to connect, it tries again, endlessly.
std::shared_ptr<READER_FAILOVER_RESULT> FAILOVER_READER_HANDLER::failover(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology) {
    auto empty_result = std::make_shared<READER_FAILOVER_RESULT>(false, nullptr, nullptr);
    if (!current_topology || current_topology->total_hosts() == 0) {
        return empty_result;
    }

    const auto start = std::chrono::steady_clock::now();
    auto global_sync = std::make_shared<FAILOVER_SYNC>(1);

    if (thread_pool.n_idle() == 0) {
        MYLOG_TRACE(logger, dbc_id, "[FAILOVER_READER_HANDLER] Resizing thread pool to %d", thread_pool.size() + 1);
        thread_pool.resize(thread_pool.size() + 1);
    }
    auto reader_result_future = thread_pool.push([=](int id) {
        while (!global_sync->is_completed()) {
            auto hosts_list = build_hosts_list(current_topology, !enable_strict_reader_failover);
            auto reader_result = get_connection_from_hosts(hosts_list, global_sync);
            if (reader_result->connected) {
                global_sync->mark_as_complete(true);
                return reader_result;
            }
            // TODO Think of changes to the strategy if it went
            // through all the hosts and did not connect.
            std::this_thread::sleep_for(std::chrono::seconds(READER_CONNECT_INTERVAL_SEC));
        }
        return empty_result;
    });

    // Wait for task complete signal with specified timeout
    global_sync->wait_and_complete(max_failover_timeout_ms);
    // Constantly polling for results until timeout
    while (true) {
        if (reader_result_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            MYLOG_TRACE(logger, dbc_id, "[FAILOVER_READER_HANDLER] Reader failover finished.");
            return reader_result_future.get();
        }

        // No result it ready, update remaining timeout
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        const auto remaining_wait_ms = max_failover_timeout_ms - duration.count();
        if (remaining_wait_ms <= 0) {
            // Reader failover timed out
            MYLOG_TRACE(logger, dbc_id, "[FAILOVER_READER_HANDLER] Reader failover timed out. Failed to connect to the reader instance.");
            return empty_result;
        }
    }
}

// Function to connect to a reader host. Often used to query/update the topology.
// If it goes through the list of readers and fails to connect, it tries again, endlessly.
// This function only tries to connect to reader hosts.
std::shared_ptr<READER_FAILOVER_RESULT> FAILOVER_READER_HANDLER::get_reader_connection(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
    std::shared_ptr<FAILOVER_SYNC> f_sync) {

    // We build a list of all readers, up then down, without writers.
    auto hosts = build_hosts_list(topology_info, false);

    while (!f_sync->is_completed()) {
        auto reader_result = get_connection_from_hosts(hosts, f_sync);
        // TODO Think of changes to the strategy if it went through all the readers and did not connect.
        if (reader_result->connected) {
            return reader_result;
        }
    }
    // Return a false result if the connection request has been cancelled.
    return std::make_shared<READER_FAILOVER_RESULT>(false, nullptr, nullptr);
}

// Function that reads the topology and builds a list of hosts to connect to, in order of priority.
// boolean include_writers indicate whether one wants to append the writers to the end of the list or not.
std::vector<std::shared_ptr<HOST_INFO>> FAILOVER_READER_HANDLER::build_hosts_list(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
    bool include_writers) {

    std::vector<std::shared_ptr<HOST_INFO>> hosts_list;

    // We split reader hosts that are marked up from those marked down.
    std::vector<std::shared_ptr<HOST_INFO>> readers_up;
    std::vector<std::shared_ptr<HOST_INFO>> readers_down;

    auto readers = topology_info->get_readers();

    for (auto reader : readers) {
        if (reader->is_host_down()) {
            readers_down.push_back(reader);
        } else {
            readers_up.push_back(reader);
        }
    }

    // Both lists of readers up and down are shuffled.
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(readers_up), std::end(readers_up), rng);
    std::shuffle(std::begin(readers_down), std::end(readers_down), rng);

    // Readers that are marked up go first, readers marked down go after.
    hosts_list.insert(hosts_list.end(), readers_up.begin(), readers_up.end());
    hosts_list.insert(hosts_list.end(), readers_down.begin(), readers_down.end());

    if (include_writers) {
        auto writers = topology_info->get_writers();
        std::shuffle(std::begin(writers), std::end(writers), rng);
        hosts_list.insert(hosts_list.end(), writers.begin(), writers.end());
    }

    return hosts_list;
}

std::shared_ptr<READER_FAILOVER_RESULT> FAILOVER_READER_HANDLER::get_connection_from_hosts(
    std::vector<std::shared_ptr<HOST_INFO>> hosts_list, std::shared_ptr<FAILOVER_SYNC> global_sync) {

    size_t total_hosts = hosts_list.size();
    size_t i = 0;

    // This loop should end once it reaches the end of the list without a successful connection.
    // The function calling it already has a neverending loop looking for a connection.
    // Ending this loop will allow the calling function to update the list or change strategy if this failed.
    while (!global_sync->is_completed() && i < total_hosts) {
        const auto start = std::chrono::steady_clock::now();

        // This boolean verifies if the next host in the list is also the last, meaning there's no host for the second thread.
        const bool odd_hosts_number = (i + 1 == total_hosts);

        auto local_sync = std::make_shared<FAILOVER_SYNC>(1);
        if (!odd_hosts_number) {
            local_sync->increment_task();
        }

        CONNECT_TO_READER_HANDLER first_connection_handler(connection_handler, topology_service, dbc_id, logger != nullptr);
        auto first_connection_result = std::make_shared<READER_FAILOVER_RESULT>(false, nullptr, nullptr);

        CONNECT_TO_READER_HANDLER second_connection_handler(connection_handler, topology_service, dbc_id, logger != nullptr);
        auto second_connection_result = std::make_shared<READER_FAILOVER_RESULT>(false, nullptr, nullptr);
        
        std::shared_ptr<HOST_INFO> first_reader_host = hosts_list.at(i);

        if (thread_pool.n_idle() <= 1) {
            int size = thread_pool.size() + 2 - thread_pool.n_idle();
            MYLOG_TRACE(logger, dbc_id,
                        "[FAILOVER_READER_HANDLER] Resizing thread pool to %d", size);
            thread_pool.resize(size);
    }

        auto first_result = thread_pool.push(std::move(first_connection_handler), first_reader_host, local_sync, first_connection_result);
        std::future<void> second_future;
        if (!odd_hosts_number) {
            auto second_reader_host = hosts_list.at(i + 1);
            second_future = thread_pool.push(std::move(second_connection_handler), second_reader_host, local_sync, second_connection_result);
        }

        // Wait for task complete signal with specified timeout
        local_sync->wait_and_complete(reader_connect_timeout_ms);

        // Constantly polling for results until timeout
        while (true) {
            // Check if first reader task result is ready
            if (first_result.valid() && first_result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                first_result.get();
                if (first_connection_result->connected) {
                    MYLOG_TRACE(logger, dbc_id,
                        "[FAILOVER_READER_HANDLER] Connected to reader: %s",
                        first_connection_result->new_host->get_host_port_pair().c_str());

                    return first_connection_result;
                }
            }

            // Check if second reader task result is ready if there is one
            if (!odd_hosts_number && second_future.valid() &&
                second_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                second_future.get();
                if (second_connection_result->connected) {
                    MYLOG_TRACE(logger, dbc_id,
                        "[FAILOVER_READER_HANDLER] Connected to reader: %s",
                        second_connection_result->new_host->get_host_port_pair().c_str());

                    return second_connection_result;
                }
            }

            // Results are ready but non has valid connection
            if (!first_result.valid() && (odd_hosts_number || !second_future.valid())) {
                break;
            }

            // No result it ready, update remaining timeout
            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            const auto remaining_wait_ms = reader_connect_timeout_ms - duration.count();
            if (remaining_wait_ms <= 0) {
                // None has connected. We move on and try new hosts.
                std::this_thread::sleep_for(std::chrono::seconds(READER_CONNECT_INTERVAL_SEC));
                break;
            }
        }
        i += 2;
    }

    // The operation was either cancelled either reached the end of the list without connecting.
    return std::make_shared<READER_FAILOVER_RESULT>(false, nullptr, nullptr);
}

// *** CONNECT_TO_READER_HANDLER
// Handler to connect to a reader host.
CONNECT_TO_READER_HANDLER::CONNECT_TO_READER_HANDLER(
    std::shared_ptr<CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    unsigned long dbc_id, bool enable_logging)
    : FAILOVER{connection_handler, topology_service, dbc_id, enable_logging} {}

CONNECT_TO_READER_HANDLER::~CONNECT_TO_READER_HANDLER() {}

void CONNECT_TO_READER_HANDLER::operator()(
    int id,
    std::shared_ptr<HOST_INFO> reader,
    std::shared_ptr<FAILOVER_SYNC> f_sync,
    std::shared_ptr<READER_FAILOVER_RESULT> result) {
    
    if (reader && !f_sync->is_completed()) {

        MYLOG_TRACE(logger, dbc_id,
                    "Thread ID %d - [CONNECT_TO_READER_HANDLER] Trying to connect to reader: %s",
                    id, reader->get_host_port_pair().c_str());

        if (connect(reader)) {
            topology_service->mark_host_up(reader);
            if (f_sync->is_completed()) {
                // If another thread finishes first, or both timeout, this thread is canceled.
                release_new_connection();
            } else {
                result->connected =true;
                result->new_host = reader;
                result->new_connection = std::move(this->new_connection);
                f_sync->mark_as_complete(true);
                MYLOG_TRACE(
                    logger, dbc_id,
                    "Thread ID %d - [CONNECT_TO_READER_HANDLER] Connected to reader: %s",
                    id, reader->get_host_port_pair().c_str());
                return;
            }
        } else {
            topology_service->mark_host_down(reader);
            MYLOG_TRACE(
                logger, dbc_id,
                "Thread ID %d - [CONNECT_TO_READER_HANDLER] Failed to connect to reader: %s",
                id, reader->get_host_port_pair().c_str());
            if (!f_sync->is_completed()) {
                f_sync->mark_as_complete(false);
            }
        }
    }

    release_new_connection();
}
