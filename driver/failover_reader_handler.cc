/**
  @file  failover_reader_handler.c
  @brief Functions for a reader failover.
*/

#include "failover.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <thread>
#include <vector>

FAILOVER_READER_HANDLER::FAILOVER_READER_HANDLER(
    std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service,
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler)
    : topology_service{topology_service},
      connection_handler{connection_handler} {}

FAILOVER_READER_HANDLER::~FAILOVER_READER_HANDLER() {}

// Function called to start the Reader Failover process.
// This process will generate a list of available hosts: First readers that are up, then readers marked as down, then writers.
// If it goes through the list and does not succeed to connect, it tries again, endlessly.
READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::failover(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology,
    const std::function<bool()> is_canceled) {
    if (!current_topology || current_topology->total_hosts() == 0) {
        return READER_FAILOVER_RESULT( false, nullptr, nullptr );
    }

    std::vector<std::shared_ptr<HOST_INFO>> hosts_list;

    while (!is_canceled()) {
        hosts_list = build_hosts_list(current_topology, true);
        auto reader_result = get_connection_from_hosts(hosts_list, is_canceled);
        if (reader_result.connected) {
            topology_service->unmark_host_down(reader_result.new_host);
            return reader_result;
        }
        // TODO Think of changes to the strategy if it went through all the hosts and did not connect.
    }
    // Return a false result if the failover has been cancelled.
    return READER_FAILOVER_RESULT(false, nullptr, nullptr);
}

// Function to connect to a reader host. Often used to query/update the topology.
// If it goes through the list of readers and fails to connect, it tries again, endlessly.
// This function only tries to connect to reader hosts.
READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::get_reader_connection(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
    const std::function<bool()> is_canceled) {

    // We build a list of all readers, up then down, without writers.
    auto hosts = build_hosts_list(topology_info, false);

    while (!is_canceled()) {
        auto reader_result = get_connection_from_hosts(hosts, is_canceled);
        // TODO Think of changes to the strategy if it went through all the readers and did not connect.
        if (reader_result.connected) {
            topology_service->unmark_host_down(reader_result.new_host);
            return reader_result;
        }
    }
    // Return a false result if the connection request has been cancelled.
    return READER_FAILOVER_RESULT(false, nullptr, nullptr);
}

// Function that reads the topology and builds a list of hosts to connect to, in order of priority.
// boolean include_writers indicate whether one wants to append the writers to the end of the list or not.
std::vector<std::shared_ptr<HOST_INFO>> FAILOVER_READER_HANDLER::build_hosts_list(
    const std::shared_ptr<CLUSTER_TOPOLOGY_INFO>& topology_info,
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

READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::get_connection_from_hosts(
    std::vector<std::shared_ptr<HOST_INFO>> hosts_list,
    const std::function<bool()> is_canceled) {

    int total_hosts = hosts_list.size();
    int i = 0;

    // This loop should end once it reaches the end of the list without a successful connection.
    // The function calling it already has a neverending loop looking for a connection.
    // Ending this loop will allow the calling function to update the list or change strategy if this failed.
    while (i < total_hosts && !is_canceled()) {
        // This boolean verifies if the next host in the list is also the last, meaning there's no host for the second thread.
        bool odd_hosts_number = (i + 1 == total_hosts);

        FAILOVER_SYNC failover_sync;

        std::shared_ptr<HOST_INFO> first_reader_host;
        CONNECT_TO_READER_HANDLER first_connection_handler(connection_handler, topology_service, connect_reader_interval_ms);
        std::future<READER_FAILOVER_RESULT> first_connection_future;
        READER_FAILOVER_RESULT first_connection_result;

        std::shared_ptr<HOST_INFO> second_reader_host;
        CONNECT_TO_READER_HANDLER second_connection_handler(connection_handler, topology_service, connect_reader_interval_ms);
        std::future<READER_FAILOVER_RESULT> second_connection_future;
        READER_FAILOVER_RESULT second_connection_result;

        first_reader_host = hosts_list.at(i);
        first_connection_future = std::async(std::launch::async, std::ref(first_connection_handler), std::cref(first_reader_host), std::ref(failover_sync));

        if (!odd_hosts_number) {
            second_reader_host = hosts_list.at(i+1);
            second_connection_future = std::async(std::launch::async, std::ref(second_connection_handler), std::cref(second_reader_host), std::ref(failover_sync));
        }

        failover_sync.wait_for_done(reader_failover_timeout_ms);

        // NOTE: Calling cancel() on the handlers below indicates to the executing handlers stop what they are doing and return.
        // If the handlers were at the point that they had a valid connection, cancel does not release this connection so it can be retrieved after
        // calls to future.get(). cancel has no effect if the thread function has already completed.
        first_connection_handler.cancel();
        if (!odd_hosts_number) {
            second_connection_handler.cancel();
        }

        if (first_connection_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            first_connection_result = first_connection_future.get();
        }
        if (!odd_hosts_number && second_connection_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            second_connection_result = second_connection_future.get();
        }

        if (first_connection_result.connected) {
            topology_service->unmark_host_down(first_reader_host);
            return first_connection_result;
        } else if (!odd_hosts_number && second_connection_result.connected) {
            topology_service->unmark_host_down(second_reader_host);
            return second_connection_result;
        }
        // None has connected. We move on and try new hosts.
        i += 2;
    }

    // The operation was either cancelled either reached the end of the list without connecting.
    return READER_FAILOVER_RESULT(false, nullptr, nullptr);
}

// *** CONNECT_TO_READER_HANDLER
// Handler to connect to a reader host.
CONNECT_TO_READER_HANDLER::CONNECT_TO_READER_HANDLER(
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service,
    int connection_interval) : FAILOVER{ connection_handler, topology_service }, reconnect_interval_ms{ connection_interval } {}

CONNECT_TO_READER_HANDLER::~CONNECT_TO_READER_HANDLER() {}

READER_FAILOVER_RESULT CONNECT_TO_READER_HANDLER::operator()(
    const std::shared_ptr<HOST_INFO>& reader, FAILOVER_SYNC& f_sync) {
    if (reader) {
        while (!is_canceled()) {
            if (connect(reader)) {
                auto new_connection = get_connection();
                f_sync.mark_as_done();
                topology_service->unmark_host_down(reader);
                return READER_FAILOVER_RESULT(true, reader, new_connection);
            }
            f_sync.mark_as_done();
            topology_service->mark_host_down(reader);
            return READER_FAILOVER_RESULT(false, nullptr, nullptr);
        }
    }
    // If another thread finishes first, or both timeout, this thread is canceled.
    f_sync.mark_as_done();
    topology_service->mark_host_down(reader);
    return READER_FAILOVER_RESULT(false, nullptr, nullptr);
}
