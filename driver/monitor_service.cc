/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "monitor_service.h"

#include "driver.h"

MONITOR_SERVICE::MONITOR_SERVICE(bool enable_logging) {
    auto mtc_use_count = MONITOR_THREAD_CONTAINER::get_singleton_use_count();
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_SERVICE] constructor: mtc_use_count = %d", mtc_use_count);

    this->thread_container = MONITOR_THREAD_CONTAINER::get_instance();
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_SERVICE] Assigned mtc with address %p", this->thread_container.get());

    mtc_use_count = MONITOR_THREAD_CONTAINER::get_singleton_use_count();
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_SERVICE] constructor: mtc_use_count = %d", mtc_use_count);
    if (enable_logging)
        this->logger = init_log_file();
}

MONITOR_SERVICE::MONITOR_SERVICE(
    std::shared_ptr<MONITOR_THREAD_CONTAINER> monitor_thread_container, bool enable_logging)
    : thread_container{std::move(monitor_thread_container)} {
 
    if (enable_logging)
        this->logger = init_log_file();
}

MONITOR_SERVICE::~MONITOR_SERVICE() {
    MYLOG_TRACE(init_log_file().get(), 0, "[~MONITOR_SERVICE] Destroying monitor service with address %p", this);
    
    auto mtc_use_count = MONITOR_THREAD_CONTAINER::get_singleton_use_count();
    MYLOG_TRACE(init_log_file().get(), 0, "[~MONITOR_SERVICE] destructor: mtc_use_count = %d", mtc_use_count);
    
    MYLOG_TRACE(init_log_file().get(), 0, "[~MONITOR_SERVICE] Reset on mtc with address %p", this->thread_container.get());
    this->thread_container.reset();
    
    mtc_use_count = MONITOR_THREAD_CONTAINER::get_singleton_use_count();
    MYLOG_TRACE(init_log_file().get(), 0, "[~MONITOR_SERVICE] destructor: after reset; mtc_use_count = %d", mtc_use_count);
}

std::shared_ptr<MONITOR_CONNECTION_CONTEXT> MONITOR_SERVICE::start_monitoring(
    DBC* dbc,
    DataSource* ds,
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds failure_detection_time,
    std::chrono::milliseconds failure_detection_interval,
    int failure_detection_count,
    std::chrono::milliseconds disposal_time) {

    if (node_keys.empty()) {
        auto msg = "[MONITOR_SERVICE] Parameter node_keys cannot be empty";
        MYLOG_TRACE(this->logger.get(), dbc ? dbc->id : 0, msg);
        throw std::invalid_argument(msg);
    }

    std::shared_ptr<MONITOR> monitor = this->thread_container->get_or_create_monitor(
        node_keys,
        std::move(host),
        disposal_time,
        ds,
        ds && ds->save_queries);

    auto context = std::make_shared<MONITOR_CONNECTION_CONTEXT>(
        dbc,
        node_keys,
        failure_detection_time,
        failure_detection_interval,
        failure_detection_count);

    monitor->start_monitoring(context);
    this->thread_container->add_task(monitor, shared_from_this());

    return context;
}

void MONITOR_SERVICE::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid context passed into stop_monitoring()");
        return;
    }

    context->invalidate();

    std::string node = this->thread_container->get_node(context->get_node_keys());
    if (node.empty()) {
        MYLOG_TRACE(
            this->logger.get(), context->get_dbc_id(),
            "[MONITOR_SERVICE] Can not find node key from context");
        return;
    }

    auto monitor = this->thread_container->get_monitor(node);
    if (monitor != nullptr) {
        monitor->stop_monitoring(context);
    }
}

void MONITOR_SERVICE::stop_monitoring_for_all_connections(std::set<std::string> node_keys) {
    std::string node = this->thread_container->get_node(node_keys);
    if (node.empty()) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid node keys passed into stop_monitoring_for_all_connections(). "
            "No existing monitor for the given set of node keys");
        return;
    }

    auto monitor = this->thread_container->get_monitor(node);
    if (monitor != nullptr) {
        monitor->clear_contexts();
        this->thread_container->reset_resource(monitor);
    }
}

void MONITOR_SERVICE::notify_unused(const std::shared_ptr<MONITOR>& monitor) const {
    if (monitor == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid monitor passed into notify_unused()");
        return;
    }

    // Remove monitor from the maps
    this->thread_container->release_resource(monitor);
}

void MONITOR_SERVICE::release_resources() {
    this->thread_container = nullptr;
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_SERVICE] about to call release_instance()");
    MONITOR_THREAD_CONTAINER::release_instance();
}
