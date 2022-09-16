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

#include "mylog.h"

std::shared_ptr<MONITOR_THREAD_CONTAINER> MONITOR_THREAD_CONTAINER::get_instance() {
    if (singleton) {
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Returning singleton");
        return singleton;
    }

    std::lock_guard<std::mutex> guard(thread_container_singleton_mutex);
    if (singleton) {
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Returning singleton");
        return singleton;
    }

    singleton = std::shared_ptr<MONITOR_THREAD_CONTAINER>(new MONITOR_THREAD_CONTAINER);
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Created singleton thread container with address %p", singleton.get());
    
    auto mtc_use_count = singleton.use_count();
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] get_instance(): mtc_use_count = %d", mtc_use_count);
    return singleton;
}

long MONITOR_THREAD_CONTAINER::get_singleton_use_count() {
    return singleton.use_count();
}

void MONITOR_THREAD_CONTAINER::release_instance() {
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Entering release_instance()");
    if (!singleton) {
        return;
    }

    singleton->stop_all_monitors();

    std::lock_guard<std::mutex> guard(thread_container_singleton_mutex);
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] release_instance(): mtc_use_count = %d", singleton.use_count());
    if (singleton.use_count() == 1) {
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Releasing resources");
        singleton->release_resources();
        singleton.reset();
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] After release: mtc_use_count = %d", singleton.use_count());
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
            MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Created monitor with address %p", monitor.get());
        }
    }

    this->populate_monitor_map(node_keys, monitor);

    return monitor;
}

void MONITOR_THREAD_CONTAINER::add_task(const std::shared_ptr<MONITOR>& monitor, const std::shared_ptr<MONITOR_SERVICE>& service) {
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] add_task(): using ms with address %p", service.get());
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] add_task(): ms_use_count = %d", service.use_count());
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

void MONITOR_THREAD_CONTAINER::reset_resource(const std::shared_ptr<MONITOR>& monitor) {
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
            MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] release_resource: erasing monitor %p from task_map", monitor.get());
            this->task_map.erase(monitor);
        }
    }

    if (this->thread_pool.n_idle() > 0) {
        this->thread_pool.resize(this->thread_pool.size() - 1);
    }
}

void MONITOR_THREAD_CONTAINER::populate_monitor_map(
    std::set<std::string> node_keys, const std::shared_ptr<MONITOR>& monitor) {

    for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        this->monitor_map[*it] = monitor;
    }
}

void MONITOR_THREAD_CONTAINER::remove_monitor_mapping(const std::shared_ptr<MONITOR>& monitor) {
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
            available_monitor->stop();
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

void MONITOR_THREAD_CONTAINER::stop_all_monitors() {
    std::vector<std::future<void>> tasks;

    {
        // Stop all monitors
        std::unique_lock<std::mutex> lock(task_map_mutex);
        for (auto const& task_pair : task_map) {
            std::shared_ptr<MONITOR> monitor = task_pair.first;
            MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Stopping monitor %p", monitor.get());
            monitor->stop();

            tasks.push_back(std::move(task_map[monitor]));
        }
    }

    // Wait for monitors to finish
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        std::future<void> task = std::move(*it);
        task.get();
    }
}

void MONITOR_THREAD_CONTAINER::release_resources() {
    {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Clearing monitor_map");
        this->monitor_map.clear();
    }

    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Stopping thread pool");
    this->thread_pool.stop(true);

    {
        std::unique_lock<std::mutex> lock(task_map_mutex);
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Clearing task_map; task_map.size() = %d", task_map.size());
        this->task_map.clear();
    }

    {
        MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR_THREAD_CONTAINER] Clearing available_monitors");
        std::unique_lock<std::mutex> lock(available_monitors_mutex);
        std::queue<std::shared_ptr<MONITOR>> empty;
        std::swap(available_monitors, empty);
    }
}
