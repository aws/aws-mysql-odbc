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

#include "monitor_thread_container.h"

MONITOR_THREAD_CONTAINER::MONITOR_THREAD_CONTAINER() : thread_pool{2} {}

std::shared_ptr<MONITOR_THREAD_CONTAINER> MONITOR_THREAD_CONTAINER::get_instance() {
    static std::shared_ptr<MONITOR_THREAD_CONTAINER> instance{new MONITOR_THREAD_CONTAINER};
    return instance;
}

std::string MONITOR_THREAD_CONTAINER::get_node(std::set<std::string> node_keys) {
    if (!this->monitor_map.empty()) {
        for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
            std::string node = *it;
            if (this->monitor_map.count(node) > 0) {
                return node;
            }
        }
    }

    return "";
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_monitor(std::string node) {
    return this->monitor_map[node];
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_or_create_monitor(
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds disposal_time,
    DataSource* ds,
    MONITOR_SERVICE* monitor_service,
    bool enable_logging) {

    std::shared_ptr<MONITOR> monitor;

    std::string node = this->get_node(node_keys);
    if (node != "") {
        monitor = this->monitor_map[node];
    }
    else {
        monitor = this->get_available_monitor();
        if (monitor == nullptr) {
            monitor = this->create_monitor(host, disposal_time, ds, monitor_service, enable_logging);
        }
    }

    this->populate_monitor_map(node_keys, monitor);

    return monitor;
}

void MONITOR_THREAD_CONTAINER::add_task(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        throw std::invalid_argument("Parameter monitor cannot be null");
    }

    if (this->task_map.count(monitor) == 0) {
        std::future<void> future = 
            this->thread_pool.push([&monitor](int id) { monitor->run(); });
        this->task_map[monitor] = &future;
    }
}

void MONITOR_THREAD_CONTAINER::reset_resource(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        return;
    }

    this->remove_monitor_mapping(monitor);

    this->available_monitors.push(monitor);
}

void MONITOR_THREAD_CONTAINER::release_resource(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        return;
    }

    this->remove_monitor_mapping(monitor);

    if (this->task_map.count(monitor) > 0) {
        std::future<void>* task = this->task_map[monitor];
        // TODO: cancel task
    }
}

void MONITOR_THREAD_CONTAINER::populate_monitor_map(
    std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor) {

    for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
        this->monitor_map[*it] = monitor;
    }
}

void MONITOR_THREAD_CONTAINER::remove_monitor_mapping(std::shared_ptr<MONITOR> monitor) {
    for (auto it = this->monitor_map.begin(); it != this->monitor_map.end();) {
        std::string node = (*it).first;
        if (this->monitor_map[node] == monitor) {
            it = this->monitor_map.erase(it);
        }
        else {
            ++it;
        }
    }
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_available_monitor() {
    if (!this->available_monitors.empty()) {
        std::shared_ptr<MONITOR> available_monitor = this->available_monitors.front();
        this->available_monitors.pop();
        
        if (!available_monitor->is_stopped()) {
            return available_monitor;
        }

        if (this->task_map.count(available_monitor) > 0) {
            // TODO: Cancel task
        }
    }

    return nullptr;
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::create_monitor(
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds disposal_time,
    DataSource* ds,
    MONITOR_SERVICE* monitor_service,
    bool enable_logging) {

    return std::make_shared<MONITOR>(host, disposal_time, ds, monitor_service, enable_logging);
}
