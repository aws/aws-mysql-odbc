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

MONITOR_SERVICE::MONITOR_SERVICE() {
    this->thread_container = std::make_shared<MONITOR_THREAD_CONTAINER>();
}

MONITOR_SERVICE::MONITOR_SERVICE(std::shared_ptr<MONITOR_THREAD_CONTAINER> monitor_thread_container) {
    this->thread_container = monitor_thread_container;
}

std::shared_ptr<MONITOR_CONNECTION_CONTEXT> MONITOR_SERVICE::start_monitoring(
    DBC* dbc,
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds failure_detection_time,
    std::chrono::milliseconds failure_detection_interval,
    int failure_detection_count,
    std::chrono::milliseconds disposal_time) {

    if (node_keys.empty()) {
        throw std::invalid_argument("Parameter node_keys cannot be empty");
    }

    std::shared_ptr<MONITOR> monitor =
        this->thread_container->get_or_create_monitor(node_keys, host, disposal_time);

    auto context = std::make_shared<MONITOR_CONNECTION_CONTEXT>(
        dbc,
        node_keys,
        failure_detection_time,
        failure_detection_interval,
        failure_detection_count);

    monitor->start_monitoring(context);
    this->thread_container->add_task(monitor);

    return context;
}

void MONITOR_SERVICE::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        // TODO: log warning
        return;
    }

    std::string node = this->thread_container->get_node(context->get_node_keys());
    if (node == "") {
        // TODO: log warning
        return;
    }

    this->thread_container->get_monitor(node)->stop_monitoring(context);
}

void MONITOR_SERVICE::stop_monitoring_for_all_connections(std::set<std::string> node_keys) {
    std::string node = this->thread_container->get_node(node_keys);
    if (node == "") {
        // TODO: log warning
        return;
    }

    auto monitor = this->thread_container->get_monitor(node);
    monitor->clear_contexts();
    this->thread_container->reset_resource(monitor);
}

void MONITOR_SERVICE::notify_unused(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        // TODO: log warning
        return;
    }

    // Remove monitor from the maps
    this->thread_container->release_resource(monitor);
}
