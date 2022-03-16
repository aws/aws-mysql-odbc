/**
  @file  failover_writer_handler.c
  @brief Failover functions.
*/

#include "failover.h"

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
            throw "Trying to cancel a failover process that is already done.";
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
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service,
    FILE* log_file, unsigned long dbc_id)
    : connection_handler{connection_handler},
      topology_service{topology_service},
      log_file{log_file},
      dbc_id{dbc_id},
      new_connection{nullptr} {}

FAILOVER::~FAILOVER() {}

bool FAILOVER::is_writer_connected() {
    return new_connection && new_connection->is_connected();
}

bool FAILOVER::connect(std::shared_ptr<HOST_INFO> host_info) {
    new_connection = connection_handler->connect(host_info);
    return is_writer_connected();
}

void FAILOVER::sleep(int miliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds));
}

// Close new connection if not needed (other task finishes and returns first)
void FAILOVER::release_new_connection() {
    if (new_connection) {
        delete new_connection;
        new_connection = nullptr;
    }
}

// ************************ RECONNECT_TO_WRITER_HANDLER
// handler reconnecting to a given host, e.g. reconnect to a current writer
RECONNECT_TO_WRITER_HANDLER::RECONNECT_TO_WRITER_HANDLER(
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service,
    int connection_interval, FILE* log_file, unsigned long dbc_id)
    : FAILOVER{connection_handler, topology_service, log_file, dbc_id},
      reconnect_interval_ms{connection_interval} {}

RECONNECT_TO_WRITER_HANDLER::~RECONNECT_TO_WRITER_HANDLER() {}

void RECONNECT_TO_WRITER_HANDLER::operator()(
    std::shared_ptr<HOST_INFO> original_writer,
    FAILOVER_SYNC& f_sync,
    WRITER_FAILOVER_RESULT& result) {

    if (original_writer) {
        MYLOG_TRACE(log_file, dbc_id,
                  "[RECONNECT_TO_WRITER_HANDLER] [TaskA] Attempting to "
                  "re-connect to the current writer instance: %s",
                  original_writer->get_host_port_pair().c_str());

        while (!f_sync.is_completed()) {
            if (connect(original_writer)) {
                auto latest_topology =
                    topology_service->get_topology(new_connection, true);
                if (latest_topology->total_hosts() > 0 &&
                    is_current_host_writer(original_writer, latest_topology)) {

                    topology_service->mark_host_up(original_writer);
                    if (f_sync.is_completed()) {
                        break;
                    }
                    result = WRITER_FAILOVER_RESULT(true, false, latest_topology,
                                                    new_connection);
                    f_sync.mark_as_complete(true);
                    MYLOG_TRACE(log_file, dbc_id, "[RECONNECT_TO_WRITER_HANDLER] [TaskA] Finished");
                    return;
                }
                release_new_connection();
            }
            sleep(reconnect_interval_ms);
        }
        MYLOG_TRACE(log_file, dbc_id, "[RECONNECT_TO_WRITER_HANDLER] [TaskA] Cancelled");
    }
    // Another thread finishes or both timeout, this thread is canceled
    release_new_connection();
    MYLOG_TRACE(log_file, dbc_id, "[RECONNECT_TO_WRITER_HANDLER] [TaskA] Finished");
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
    std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service,
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology,
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
    int connection_interval, FILE* log_file, unsigned long dbc_id)
    : FAILOVER{connection_handler, topology_service, log_file, dbc_id},
      current_topology{current_topology},
      reader_handler{reader_handler},
      read_topology_interval_ms{connection_interval} {}

WAIT_NEW_WRITER_HANDLER::~WAIT_NEW_WRITER_HANDLER() {}

void WAIT_NEW_WRITER_HANDLER::operator()(
    const std::shared_ptr<HOST_INFO>& original_writer,
    FAILOVER_SYNC& f_sync,
    WRITER_FAILOVER_RESULT& result) {

    MYLOG_TRACE(log_file, dbc_id, "[WAIT_NEW_WRITER_HANDLER] [TaskB] Attempting to connect to a new writer instance");
    
    while (!f_sync.is_completed()) {
        if (!is_writer_connected()) {
            connect_to_reader(f_sync);
            refresh_topology_and_connect_to_new_writer(original_writer, f_sync);
            clean_up_reader_connection();
        } else {
            result = WRITER_FAILOVER_RESULT(true, true, current_topology,
                                            new_connection);
            f_sync.mark_as_complete(true);
            MYLOG_TRACE(log_file, dbc_id, "[WAIT_NEW_WRITER_HANDLER] [TaskB] Finished");
            return;
        }
    }
    MYLOG_TRACE(log_file, dbc_id, "[WAIT_NEW_WRITER_HANDLER] [TaskB] Cancelled");

    // Another thread finishes or both timeout, this thread is canceled
    clean_up_reader_connection();
    release_new_connection();
    MYLOG_TRACE(log_file, dbc_id, "[WAIT_NEW_WRITER_HANDLER] [TaskB] Finished");
}

// Connect to a reader to later retrieve the latest topology
void WAIT_NEW_WRITER_HANDLER::connect_to_reader(FAILOVER_SYNC& f_sync) {
    while (!f_sync.is_completed()) {
        auto connection_result = reader_handler->get_reader_connection(current_topology, f_sync);
        if (connection_result.connected && connection_result.new_connection->is_connected()) {
            reader_connection = connection_result.new_connection;
            current_reader_host = connection_result.new_host;
            MYLOG_TRACE(
                log_file, dbc_id,
                "[WAIT_NEW_WRITER_HANDLER] [TaskB] Connected to reader: %s",
                connection_result.new_host->get_host_port_pair().c_str());
            break;
        }
        MYLOG_TRACE(log_file, dbc_id, "[WAIT_NEW_WRITER_HANDLER] [TaskB] Failed to connect to any reader.");
    }
}

// Use just connected reader to refresh topology and try to connect to a new writer
void WAIT_NEW_WRITER_HANDLER::refresh_topology_and_connect_to_new_writer(
    const std::shared_ptr<HOST_INFO>& original_writer, FAILOVER_SYNC& f_sync) {
    while (!f_sync.is_completed()) {
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
    const std::shared_ptr<HOST_INFO>& writer_candidate) {

    MYLOG_TRACE(log_file, dbc_id,
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
        delete reader_connection;
        reader_connection = nullptr;
    }
}

// ************************** FAILOVER_WRITER_HANDLER  **************************

FAILOVER_WRITER_HANDLER::FAILOVER_WRITER_HANDLER(
    std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service,
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    int writer_failover_timeout_ms, int read_topology_interval_ms,
    int reconnect_writer_interval_ms, FILE* log_file, unsigned long dbc_id)
    : connection_handler{connection_handler},
      topology_service{topology_service},
      reader_handler{reader_handler},
      writer_failover_timeout_ms{writer_failover_timeout_ms},
      read_topology_interval_ms{read_topology_interval_ms},
      reconnect_writer_interval_ms{reconnect_writer_interval_ms},
      log_file{log_file},
      dbc_id{dbc_id} {}

FAILOVER_WRITER_HANDLER::~FAILOVER_WRITER_HANDLER() {}

WRITER_FAILOVER_RESULT FAILOVER_WRITER_HANDLER::failover(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology) {
    
    if (!current_topology || current_topology->total_hosts() == 0) {
        MYLOG_TRACE(log_file, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Failover was called with "
                    "an invalid (null or empty) topology");
        return WRITER_FAILOVER_RESULT(false, false, nullptr, nullptr);
    }

    FAILOVER_SYNC failover_sync(2);
    // Constructing the function objects
    RECONNECT_TO_WRITER_HANDLER reconnect_handler(
        connection_handler, topology_service, reconnect_writer_interval_ms,
        log_file, dbc_id);
    WAIT_NEW_WRITER_HANDLER new_writer_handler(
        connection_handler, topology_service, current_topology, reader_handler,
        read_topology_interval_ms, log_file, dbc_id);

    auto original_writer = current_topology->get_writer();
    topology_service->mark_host_down(original_writer);

    auto reconnect_result = WRITER_FAILOVER_RESULT(false, false, nullptr, nullptr);
    auto new_writer_result = WRITER_FAILOVER_RESULT(false, false, nullptr, nullptr);

    // Try reconnecting to the original writer host
    auto reconnect_future = std::async(std::launch::async, std::ref(reconnect_handler),
                                       original_writer, std::ref(failover_sync),
                                       std::ref(reconnect_result));
    
    // Concurrently see if topology has changed and try connecting to a new writer
    auto new_writer_future = std::async(std::launch::async, std::ref(new_writer_handler),
                                        std::cref(original_writer), std::ref(failover_sync),
                                        std::ref(new_writer_result));

    failover_sync.wait_and_complete(writer_failover_timeout_ms);

    if (reconnect_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        reconnect_future.get();
    }

    if (new_writer_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        new_writer_future.get();
    }

    if (reconnect_result.connected) {
        MYLOG_TRACE(log_file, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Successfully re-connected to the current writer instance: %s",
                    reconnect_result.new_topology->get_writer()->get_host_port_pair().c_str());
        return reconnect_result;
    } else if (new_writer_result.connected) {
        MYLOG_TRACE(log_file, dbc_id,
                    "[FAILOVER_WRITER_HANDLER] Successfully connected to the new writer instance: %s",
                    new_writer_result.new_topology->get_writer()->get_host_port_pair().c_str());
        return new_writer_result;
    }

    // timeout
    MYLOG_TRACE(log_file, dbc_id, "[FAILOVER_WRITER_HANDLER] Failed to connect to the writer instance.");
    return WRITER_FAILOVER_RESULT(false, false, nullptr, nullptr);
}
