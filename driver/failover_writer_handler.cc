/**
  @file  failover_writer_handler.c
  @brief Failover functions.
*/

#include <chrono>
#include <thread>

#include "driver.h"

// **** FAILOVER_SYNC ***************************************
// used for thread synchronization
FAILOVER_SYNC::FAILOVER_SYNC() : done_{false} {}

void FAILOVER_SYNC::mark_as_done() {
    std::lock_guard<std::mutex> lock(mutex_);
    done_ = true;
    cv.notify_one();
}

void FAILOVER_SYNC::wait_for_done() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv.wait(lock, [this] { return done_; });
}

void FAILOVER_SYNC::wait_for_done(int milliseconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return done_; });
}

// ************* FAILOVER ***********************************
// Base class of two writer failover task handlers
FAILOVER::FAILOVER(
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service)
    : connection_handler{connection_handler},
      topology_service{topology_service},
      new_connection{nullptr},
      canceled{false} {}
FAILOVER::~FAILOVER() { close_connection(); }

void FAILOVER::cancel() { canceled = true; }

bool FAILOVER::is_canceled() { return canceled; }

bool FAILOVER::is_writer_connected() { return new_connection != nullptr; }

bool FAILOVER::connect(std::shared_ptr<HOST_INFO> host_info) {
    new_connection =
        std::make_shared<MYSQL>(*connection_handler->connect(host_info.get()));
    return new_connection != nullptr;
}

std::shared_ptr<MYSQL> FAILOVER::get_connection() { return new_connection; }

void FAILOVER::close_connection() { mysql_close(new_connection.get()); }

void FAILOVER::sleep(int miliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds));
}

// ************************ RECONNECT_TO_WRITER_HANDLER
// handler reconnecting to a given host, e.g. reconnect to a current writer
RECONNECT_TO_WRITER_HANDLER::RECONNECT_TO_WRITER_HANDLER(
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service, int connection_interval)
    : FAILOVER{connection_handler, topology_service},
      reconnect_interval_ms{connection_interval} {}

RECONNECT_TO_WRITER_HANDLER::~RECONNECT_TO_WRITER_HANDLER() {}

WRITER_FAILOVER_RESULT RECONNECT_TO_WRITER_HANDLER::operator()(
    const std::shared_ptr<HOST_INFO>& original_writer, FAILOVER_SYNC& f_sync) {
    if (original_writer) {
        while (!is_canceled()) {
            if (connect(original_writer)) {
                auto conn = get_connection();
                auto latest_topology =
                    topology_service->get_topology(conn, true);
                if (latest_topology->total_hosts() > 0 &&
                    is_current_host_writer(original_writer, latest_topology)) {
                    topology_service->unmark_host_down(original_writer);
                    return WRITER_FAILOVER_RESULT{true, false, latest_topology,
                                                  conn};
                }
            }
            sleep(reconnect_interval_ms);
        }
    }
    f_sync.mark_as_done();
    return WRITER_FAILOVER_RESULT{false, false, nullptr, nullptr};
}

bool RECONNECT_TO_WRITER_HANDLER::is_current_host_writer(
    const std::shared_ptr<HOST_INFO>& original_writer,
    const std::shared_ptr<CLUSTER_TOPOLOGY_INFO>& latest_topology) {
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
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology,
    FAILOVER_READER_HANDLER& failover_reader_handler, int connection_interval)
    : FAILOVER{connection_handler, topology_service},
      current_topology{current_topology},
      reader_handler{failover_reader_handler},
      read_topology_interval_ms{connection_interval} {}

WAIT_NEW_WRITER_HANDLER::~WAIT_NEW_WRITER_HANDLER() {}

WRITER_FAILOVER_RESULT WAIT_NEW_WRITER_HANDLER::operator()(
    const std::shared_ptr<HOST_INFO>& original_writer, FAILOVER_SYNC& f_sync) {
    while (!is_canceled()) {
        if (!is_writer_connected()) {
            connect_to_reader();
            refresh_topology_and_connect_to_new_writer(original_writer);
            clean_up_reader_connection();
        } else {
            f_sync.mark_as_done();
            return WRITER_FAILOVER_RESULT{true, true, current_topology,
                                          current_connection};
        }
    }
    clean_up_reader_connection();
    // Another thread finishes, this thread is canceled
    f_sync.mark_as_done();
    return WRITER_FAILOVER_RESULT{false, false, nullptr, nullptr};
}

// Connect to a reader to later retrieve the latest topology
void WAIT_NEW_WRITER_HANDLER::connect_to_reader() {
    while (!is_canceled()) {
        auto connection_result =
            reader_handler.getReaderConnection(current_topology);
        if (connection_result.is_connected &&
            connection_result.new_host_connection != nullptr) {
            reader_connection = connection_result.new_host_connection;
            current_reader_host = connection_result.new_host;
            break;
        }
    }
}

// Use just connected reader to refresh topology and try to connect to a new
// writer
void WAIT_NEW_WRITER_HANDLER::refresh_topology_and_connect_to_new_writer(
    const std::shared_ptr<HOST_INFO>& original_writer) {
    while (!is_canceled()) {
        auto latest_topology =
            topology_service->get_topology(reader_connection, true);
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
    const std::shared_ptr<HOST_INFO>& writer_candidate) {
    if (HOST_INFO::is_host_same(writer_candidate, current_reader_host)) {
        current_connection = reader_connection;
    } else if (connect(writer_candidate)) {
        current_connection = get_connection();
    } else {
        topology_service->mark_host_down(writer_candidate);
        return false;
    }
    topology_service->unmark_host_down(writer_candidate);
    return true;
}

// Close reader connection if not needed(open and not the same as current
// connection)
void WAIT_NEW_WRITER_HANDLER::clean_up_reader_connection() {
    if (reader_connection && current_connection != reader_connection) {
        connection_handler->release_connection(reader_connection.get());
    }
}

// ****************FAILOVER_WRITER_HANDLER  ************************************
FAILOVER_WRITER_HANDLER::FAILOVER_WRITER_HANDLER(
    TOPOLOGY_SERVICE* topology_service,
    FAILOVER_READER_HANDLER* failover_reader_handler)
    : read_topology_interval_ms{5000}  // 5 sec
{}

FAILOVER_WRITER_HANDLER::~FAILOVER_WRITER_HANDLER() {}

void FAILOVER_WRITER_HANDLER::sleep(int miliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds));
}

// see how this is used, but potentially set the TOPOLOGY_SERVICE* ts,
// FAILOVER_CONNECTION_HANDLER* conn_handler in constructor and use memeber
// variables
WRITER_FAILOVER_RESULT FAILOVER_WRITER_HANDLER::failover(
    TOPOLOGY_SERVICE* topology_service,
    FAILOVER_CONNECTION_HANDLER* conn_handler,
    FAILOVER_READER_HANDLER& failover_reader_handler) {
    // TODO implement
    return WRITER_FAILOVER_RESULT{};
}
