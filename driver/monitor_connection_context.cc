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

#include "monitor_connection_context.h"
#include "driver.h"

#include <algorithm>
#include <mutex>

MONITOR_CONNECTION_CONTEXT::MONITOR_CONNECTION_CONTEXT(DBC* connection_to_abort,
    std::set<std::string> node_keys,
    std::chrono::milliseconds failure_detection_time,
    std::chrono::milliseconds failure_detection_interval,
    int failure_detection_count) : connection_to_abort{connection_to_abort},
                                   node_keys{node_keys},
                                   failure_detection_time{failure_detection_time},
                                   failure_detection_interval{failure_detection_interval},
                                   failure_detection_count{failure_detection_count},
                                   failure_count{0},
                                   node_unhealthy{false} {
    if (connection_to_abort && connection_to_abort->ds && connection_to_abort->ds->save_queries)
        this->logger = init_log_file();
}

MONITOR_CONNECTION_CONTEXT::~MONITOR_CONNECTION_CONTEXT() {}

std::chrono::steady_clock::time_point MONITOR_CONNECTION_CONTEXT::get_start_monitor_time() {
    return start_monitor_time;
}

void MONITOR_CONNECTION_CONTEXT::set_start_monitor_time(std::chrono::steady_clock::time_point time) {
    start_monitor_time = time;
}

std::set<std::string> MONITOR_CONNECTION_CONTEXT::get_node_keys() {
    return node_keys;
}

std::chrono::milliseconds MONITOR_CONNECTION_CONTEXT::get_failure_detection_time() {
    return failure_detection_time;
}

std::chrono::milliseconds MONITOR_CONNECTION_CONTEXT::get_failure_detection_interval() {
    return failure_detection_interval;
}

int MONITOR_CONNECTION_CONTEXT::get_failure_detection_count() {
    return failure_detection_count;
}

int MONITOR_CONNECTION_CONTEXT::get_failure_count() {
    return failure_count;
}

void MONITOR_CONNECTION_CONTEXT::set_failure_count(int count) {
    failure_count = count;
}

void MONITOR_CONNECTION_CONTEXT::increment_failure_count() {
    failure_count++;
}

void MONITOR_CONNECTION_CONTEXT::set_invalid_node_start_time(std::chrono::steady_clock::time_point time) {
    invalid_node_start_time = time;
}

void MONITOR_CONNECTION_CONTEXT::reset_invalid_node_start_time() {
    std::chrono::steady_clock::time_point timestamp_zero{};
    invalid_node_start_time = timestamp_zero;
}

bool MONITOR_CONNECTION_CONTEXT::is_invalid_node_start_time_defined() {
    std::chrono::steady_clock::time_point timestamp_zero{};
    return invalid_node_start_time > timestamp_zero;
}

std::chrono::steady_clock::time_point MONITOR_CONNECTION_CONTEXT::get_invalid_node_start_time() {
    return invalid_node_start_time;
}

bool MONITOR_CONNECTION_CONTEXT::is_node_unhealthy() {
    return node_unhealthy;
}

void MONITOR_CONNECTION_CONTEXT::set_node_unhealthy(bool node) {
    node_unhealthy = node;
}

bool MONITOR_CONNECTION_CONTEXT::is_active_context() {
    return active_context;
}

void MONITOR_CONNECTION_CONTEXT::invalidate() {
    active_context = false;
}

DBC* MONITOR_CONNECTION_CONTEXT::get_connection_to_abort() {
    return connection_to_abort;
}

unsigned long MONITOR_CONNECTION_CONTEXT::get_dbc_id() {
    return connection_to_abort ? connection_to_abort->id : 0;
}

// Update whether the connection is still valid if the total elapsed time has passed the grace period.
void MONITOR_CONNECTION_CONTEXT::update_connection_status(
    std::chrono::steady_clock::time_point status_check_start_time,
    std::chrono::steady_clock::time_point current_time,
    bool is_valid) {
    
    if (!is_active_context()) {
      return;
    }

    auto total_elapsed_time = current_time - get_start_monitor_time();

    if (total_elapsed_time > get_failure_detection_time()) {
      set_connection_valid(is_valid, status_check_start_time, current_time);
    }
}

// Set whether the connection to the server is still valid based on the monitoring settings.
void MONITOR_CONNECTION_CONTEXT::set_connection_valid(
    bool connection_valid,
    std::chrono::steady_clock::time_point status_check_start_time,
    std::chrono::steady_clock::time_point current_time) {

    const auto node_keys_str = build_node_keys_str();
    if (!connection_valid) {
        increment_failure_count();

        if (!is_invalid_node_start_time_defined()) {
            set_invalid_node_start_time(status_check_start_time);
        }

        const auto invalid_node_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - get_invalid_node_start_time());

        const auto max_invalid_node_duration = get_failure_detection_interval() * (std::max)(0, get_failure_detection_count());

        if (invalid_node_duration_ms >= max_invalid_node_duration) {
            MYLOG_TRACE(logger.get(), get_dbc_id(), "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *dead*.", node_keys_str.c_str());
            set_node_unhealthy(true);
            abort_connection();
            return;
        }

        MYLOG_TRACE(
            logger.get(), get_dbc_id(),
            "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *not responding* (%d).", node_keys_str.c_str(), get_failure_count());
        return;
    }

    set_failure_count(0);
    reset_invalid_node_start_time();
    set_node_unhealthy(false);
    MYLOG_TRACE(logger.get(), get_dbc_id(), "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *alive*.", node_keys_str.c_str());
}

void MONITOR_CONNECTION_CONTEXT::abort_connection() {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((!get_connection_to_abort()) || (!is_active_context())) {
        return;
    }
    connection_to_abort->mysql_proxy->close_socket();
}

std::string MONITOR_CONNECTION_CONTEXT::build_node_keys_str() {
    if (node_keys.empty()) {
        return "[]";
    }

    auto it = node_keys.begin();
    std::string node_keys_str = '[' +(*it);
    ++it;
    while (it != node_keys.end()) {
        node_keys_str += ", " + *it;
        ++it;
    }
    node_keys_str += ']';

    return node_keys_str;
}
