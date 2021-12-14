/**
  @file  failover_reader_handler.c
  @brief Failover functions.
*/

#include "failover.h"

#include <algorithm>
#include <random>
#include <vector>

FAILOVER_READER_HANDLER::FAILOVER_READER_HANDLER(
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler)
    : topology_service{topology_service},
      connection_handler{connection_handler} {}

FAILOVER_READER_HANDLER::~FAILOVER_READER_HANDLER() {}

// Called to start Reader Failover Process. This process tries to connect to any
// reader. If no reader is available then driver may also try to connect to a
// writer host, down hosts, and the current reader host.
READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::failover(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
    const std::function<bool()> is_canceled) {

    auto hosts = build_hosts_list(topology_info, true);

    while (!is_canceled()) {
        auto reader_result = get_connection_from_hosts(hosts, is_canceled);
        // TODO What if not connected
        if (reader_result.connected) {
            return reader_result;
        }
        // TODO remove the element we tried before new iteration
    }
    // Cancelled
    return READER_FAILOVER_RESULT{false, nullptr, nullptr};
}

// Called to get any available reader connection. If no reader is available then
// result of proces is unsuccessful. This process will not attempt to connect to
// the writer.
READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::get_reader_connection(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
    const std::function<bool()> is_canceled) {

    auto hosts = build_hosts_list(topology_info, false);
    return get_connection_from_hosts(hosts, is_canceled);
}

std::vector<std::shared_ptr<HOST_INFO>>
FAILOVER_READER_HANDLER::build_hosts_list(
    const std::shared_ptr<CLUSTER_TOPOLOGY_INFO>& topology_info,
    bool contain_writers) {

    std::vector<std::shared_ptr<HOST_INFO>> hosts_list;
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

    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(readers_up), std::end(readers_up), rng);
    std::shuffle(std::begin(readers_down), std::end(readers_down), rng);

    hosts_list.insert(hosts_list.end(), readers_up.begin(), readers_up.end());
    hosts_list.insert(hosts_list.end(), readers_down.begin(), readers_down.end());

    if (contain_writers) {
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

    // TODO try to connect to two readers in separete threads and select the
    // fastest

    int i = 0;
    std::shared_ptr<HOST_INFO> new_host;
    while (i < total_hosts && !is_canceled()) {
        auto new_connection = connection_handler->connect(hosts_list.at(i));
        if (!new_connection->is_null()) {
            new_host = hosts_list.at(i);
            return READER_FAILOVER_RESULT{true, new_host, new_connection};
        }
        if (i >= total_hosts) {
            i = 0;
        } else {
            i++;
        }
    }
    // Cancelled
    return READER_FAILOVER_RESULT{false, nullptr, nullptr};
}
