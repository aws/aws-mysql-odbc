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

std::shared_ptr<MONITOR_THREAD_CONTAINER> MONITOR_THREAD_CONTAINER::get_instance() {
    if (singleton) {
        return singleton;
    }

    std::lock_guard<std::mutex> guard(thread_container_singleton_mutex);
    if (singleton) {
        return singleton;
    }

    singleton = std::shared_ptr<MONITOR_THREAD_CONTAINER>(new MONITOR_THREAD_CONTAINER);
    return singleton;
}

void MONITOR_THREAD_CONTAINER::release_instance() {
    if (!singleton) {
        return;
    }

    std::lock_guard<std::mutex> guard(thread_container_singleton_mutex);
    if (singleton.use_count() == 1) {
        singleton->release_resources();
        singleton.reset();
    }
}

std::string MONITOR_THREAD_CONTAINER::get_node(std::set<std::string> node_keys) {
    std::unique_lock<std::mutex> lock(monitor_map_mutex);
    if (!this->monitor_map.empty()) {
        for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
            std::string node = *it;
            if (this->monitor_map.count(node) > 0) {
                return node;
            }
        }
    }

    return std::string{};
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_monitor(std::string node) {
    std::unique_lock<std::mutex> lock(monitor_map_mutex);
    return this->monitor_map.count(node) > 0 ? this->monitor_map.at(node) : nullptr;
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_or_create_monitor(
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds disposal_time,
    DataSource* ds,
    bool enable_logging) {

    std::shared_ptr<MONITOR> monitor;

    std::unique_lock<std::mutex> lock(mutex_);
    std::string node = this->get_node(node_keys);
    if (!node.empty()) {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        monitor = this->monitor_map[node];
    }
    else {
        monitor = this->get_available_monitor();
        if (monitor == nullptr) {
            monitor = this->create_monitor(std::move(host), disposal_time, ds, enable_logging);
        }
    }

    this->populate_monitor_map(node_keys, monitor);

    return monitor;
}

void MONITOR_THREAD_CONTAINER::add_task(std::shared_ptr<MONITOR> monitor, std::shared_ptr<MONITOR_SERVICE> service) {
    if (monitor == nullptr || service == nullptr) {
        throw std::invalid_argument("Invalid parameters passed into MONITOR_THREAD_CONTAINER::add_task()");
    }

    std::unique_lock<std::mutex> lock(task_map_mutex);
    if (this->task_map.count(monitor) == 0) {
        this->thread_pool.resize(this->thread_pool.size() + 1);
        auto run_monitor = [monitor, service](int id) { monitor->run(service); };
        this->task_map[monitor] = this->thread_pool.push(run_monitor);
    }
}

void MONITOR_THREAD_CONTAINER::reset_resource(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        return;
    }

    this->remove_monitor_mapping(monitor);

    std::unique_lock<std::mutex> lock(available_monitors_mutex);
    this->available_monitors.push(monitor);
}

void MONITOR_THREAD_CONTAINER::release_resource(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        return;
    }

    this->remove_monitor_mapping(monitor);

    {
        std::unique_lock<std::mutex> lock(task_map_mutex);
        if (this->task_map.count(monitor) > 0) {
            this->task_map.erase(monitor);
        }
    }

    if (this->thread_pool.n_idle() > 0) {
        this->thread_pool.resize(this->thread_pool.size() - 1);
    }
}

void MONITOR_THREAD_CONTAINER::populate_monitor_map(
    std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor) {

    for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        this->monitor_map[*it] = monitor;
    }
}

void MONITOR_THREAD_CONTAINER::remove_monitor_mapping(std::shared_ptr<MONITOR> monitor) {
    std::unique_lock<std::mutex> lock(monitor_map_mutex);
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
    std::unique_lock<std::mutex> lock(available_monitors_mutex);
    if (!this->available_monitors.empty()) {
        std::shared_ptr<MONITOR> available_monitor = this->available_monitors.front();
        this->available_monitors.pop();
        
        if (!available_monitor->is_stopped()) {
            return available_monitor;
        }

        std::unique_lock<std::mutex> lock(task_map_mutex);
        if (this->task_map.count(available_monitor) > 0) {
            this->task_map.erase(available_monitor);
        }
    }

    return nullptr;
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::create_monitor(
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds disposal_time,
    DataSource* ds,
    bool enable_logging) {

    return std::make_shared<MONITOR>(host, disposal_time, ds, enable_logging);
}

void MONITOR_THREAD_CONTAINER::release_resources() {
    {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        this->monitor_map.clear();
    }
    {
        std::unique_lock<std::mutex> lock(task_map_mutex);
        this->task_map.clear();
    }

    this->thread_pool.stop();
}
