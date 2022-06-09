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

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_monitor(std::string node) {
    return this->monitor_map[node];
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_or_create_monitor(
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds disposal_time) {

    std::string node = this->get_node(node_keys, *node_keys.begin());
    std::shared_ptr<MONITOR> monitor = this->monitor_map[node];
    if (monitor == nullptr) {
        monitor = this->create_monitor(host, disposal_time);
    }

    this->populate_monitor_map(node_keys, monitor);

    return monitor;
}

void MONITOR_THREAD_CONTAINER::add_task(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        return;
    }

    if (this->task_map.count(monitor) == 0) {
        // TODO: create task and add it to map
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

std::string MONITOR_THREAD_CONTAINER::get_node(std::set<std::string> node_keys) {
    return this->get_node(node_keys, "");
}

std::string MONITOR_THREAD_CONTAINER::get_node(std::set<std::string> node_keys, std::string default_value) {
    if (this->monitor_map.size() > 0) {
        for (auto it = node_keys.begin(); it != node_keys.end(); it++) {
            std::string node = *it;
            if (this->monitor_map.count(node) > 0) {
                return node;
            }
        }
    }

    return default_value;
}

void MONITOR_THREAD_CONTAINER::populate_monitor_map(
    std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor) {

    for (auto it = node_keys.begin(); it != node_keys.end(); it++) {
        this->monitor_map[*it] = monitor;
    }
}

void MONITOR_THREAD_CONTAINER::remove_monitor_mapping(std::shared_ptr<MONITOR> monitor) {
    for (auto it = this->monitor_map.begin(); it != this->monitor_map.end(); it++) {
        std::string node = (*it).first;
        if (this->monitor_map[node] == monitor) {
            this->monitor_map.erase(node);
        }
    }
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::create_monitor(
    std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time) {

    return std::make_shared<MONITOR>(host, disposal_time);
}
