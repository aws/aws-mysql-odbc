/**
  @file  failover_writer_handler.c
  @brief Failover functions.
*/

#include "driver.h"

#include <thread>
#include <chrono>

// **** FAILOVER_SYNC ***************************************
//used for thread syncronization
FAILOVER_SYNC::FAILOVER_SYNC() : done_{ false } {}

void FAILOVER_SYNC::mark_as_done() {
    // TODO implement
}

void FAILOVER_SYNC::wait_for_done() {
    // TODO implement
}

void FAILOVER_SYNC::wait_for_done(int milliseconds) {
    // TODO implement
}

bool FAILOVER_SYNC::is_done() {
    return done_;
}

// ************* FAILOVER ***********************************
// Base class of two writer failover task handlers
FAILOVER::FAILOVER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler, std::shared_ptr<TOPOLOGY_SERVICE> topology_service)
    : connection_handler{ connection_handler }, topology_service{ topology_service }, new_connection{ nullptr }, canceled{ false } {
}
FAILOVER::~FAILOVER() {
    close_connection();
}

void FAILOVER::cancel() { 
    canceled = true; 
}

bool FAILOVER::is_canceled() { 
    return canceled;
}

bool FAILOVER::connected() {
    return new_connection != nullptr;
}

bool FAILOVER::connect(std::shared_ptr<HOST_INFO> host_info) {
    new_connection = std::make_shared<MYSQL>(*connection_handler->connect(host_info.get()));
    return new_connection != nullptr;
}

std::shared_ptr<MYSQL> FAILOVER::get_connection() {
    return new_connection;
}

void FAILOVER::close_connection() {
    mysql_close(new_connection.get());
}

void FAILOVER::sleep(int miliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds));
}

// ************************ RECONNECT_TO_WRITER_HANDLER
//handler reconnecting to a given host, e.g. reconnect to a current writer
RECONNECT_TO_WRITER_HANDLER::RECONNECT_TO_WRITER_HANDLER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler, std::shared_ptr<TOPOLOGY_SERVICE> topology_service, int connection_interval)
    : FAILOVER{ connection_handler, topology_service }, reconnect_interval_ms{ connection_interval } {
}

RECONNECT_TO_WRITER_HANDLER::~RECONNECT_TO_WRITER_HANDLER() {
}

WRITER_FAILOVER_RESULT RECONNECT_TO_WRITER_HANDLER::operator()(std::shared_ptr<HOST_INFO> original_writer, FAILOVER_SYNC& f_sync) {
    if (original_writer) {
        while (!is_canceled()) {
            if (connect(original_writer)) {
                auto conn = get_connection();
                auto latest_topology = topology_service->get_topology(conn, true);
                if (latest_topology->total_hosts() > 0 && is_current_host_writer(original_writer, std::move(latest_topology))) {
                    topology_service->unmark_host_down(original_writer);
                    return WRITER_FAILOVER_RESULT{ true, false, std::move(latest_topology), conn };
                }
            }
            sleep(reconnect_interval_ms);
        }
    }
    f_sync.mark_as_done();
    return WRITER_FAILOVER_RESULT{ false, false, nullptr , nullptr };
}

bool RECONNECT_TO_WRITER_HANDLER::is_current_host_writer(std::shared_ptr<HOST_INFO> original_writer, std::unique_ptr<CLUSTER_TOPOLOGY_INFO> latest_topology) {
    auto original_instance = original_writer->instance_name;
    if (original_instance.empty())
        return false;
    auto latest_writer = latest_topology->get_writer();
    auto latest_instance = latest_writer->instance_name;
    return latest_instance == original_instance;
}

// ************************ WAIT_NEW_WRITER_HANDLER

WAIT_NEW_WRITER_HANDLER::WAIT_NEW_WRITER_HANDLER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler, std::shared_ptr<TOPOLOGY_SERVICE> topology_service, FAILOVER_READER_HANDLER& failover_reader_handler, int connection_interval)
    : FAILOVER{ connection_handler, topology_service }, reader_handler{ failover_reader_handler }, read_topology_interval_ms{ connection_interval } {
}

WAIT_NEW_WRITER_HANDLER::~WAIT_NEW_WRITER_HANDLER() {
    // TODO
}

void WAIT_NEW_WRITER_HANDLER::operator()(FAILOVER_SYNC& f_sync) {   
    // TODO implement
}

std::shared_ptr<MYSQL> WAIT_NEW_WRITER_HANDLER::get_reader_connection() {
    // TODO implement
    return NULL;
}

bool WAIT_NEW_WRITER_HANDLER::refresh_topology_and_connect_to_new_writer(std::shared_ptr<MYSQL> reader_connection) {
    // TODO implement
    return false;
}

// ****************FAILOVER_WRITER_HANDLER  ************************************
FAILOVER_WRITER_HANDLER::FAILOVER_WRITER_HANDLER(TOPOLOGY_SERVICE* topology_service, FAILOVER_READER_HANDLER* failover_reader_handler)
    : read_topology_interval_ms{ 5000 } // 5 sec
{}

FAILOVER_WRITER_HANDLER::~FAILOVER_WRITER_HANDLER() {}

bool FAILOVER_WRITER_HANDLER::is_host_same(HOST_INFO* h1, HOST_INFO* h2) {
    // TODO implement
    return false;
}

void FAILOVER_WRITER_HANDLER::sleep(int miliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds));
}

// see how this is used, but potentially set the TOPOLOGY_SERVICE* ts, FAILOVER_CONNECTION_HANDLER* conn_handler
// in constructor and use memeber variables
WRITER_FAILOVER_RESULT FAILOVER_WRITER_HANDLER::failover(TOPOLOGY_SERVICE* topology_service, FAILOVER_CONNECTION_HANDLER* conn_handler, FAILOVER_READER_HANDLER& failover_reader_handler)
{
    // TODO implement
    return WRITER_FAILOVER_RESULT{};
}
