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

#include <algorithm>

MONITOR_CONNECTION_CONTEXT::MONITOR_CONNECTION_CONTEXT(DBC* connection_to_abort,
    std::set<std::string> node_keys,
    int failure_detection_interval_ms,
    int failure_detection_time_ms,
    int failure_detection_count) : connection_to_abort{connection_to_abort},
                                   node_keys{node_keys},
                                   failure_detection_interval_ms{failure_detection_interval_ms},
                                   failure_detection_time_ms{failure_detection_time_ms},
                                   failure_detection_count{failure_detection_count} {}

MONITOR_CONNECTION_CONTEXT::~MONITOR_CONNECTION_CONTEXT() {}

long MONITOR_CONNECTION_CONTEXT::get_start_monitor_time() {
    return start_monitor_time;
}

void MONITOR_CONNECTION_CONTEXT::set_start_monitor_time(long time) {
    start_monitor_time = time;
}

std::set<std::string> MONITOR_CONNECTION_CONTEXT::get_node_keys() {
    return node_keys;
}

int MONITOR_CONNECTION_CONTEXT::get_failure_detection_time_ms() {
    return failure_detection_time_ms;
}

int MONITOR_CONNECTION_CONTEXT::get_failure_detection_interval_ms() {
    return failure_detection_interval_ms;
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

void MONITOR_CONNECTION_CONTEXT::set_invalid_node_start_time(long time_ms) {
    invalid_node_start_time = time_ms;
}

void MONITOR_CONNECTION_CONTEXT::reset_invalid_node_start_time() {
    invalid_node_start_time = 0;
}

bool MONITOR_CONNECTION_CONTEXT::is_invalid_node_start_time_defined() {
    return invalid_node_start_time > 0;
}

long MONITOR_CONNECTION_CONTEXT::get_invalid_node_start_time() {
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

// TODO synchronized
void MONITOR_CONNECTION_CONTEXT::abort_connection() {
    if ((get_connection_to_abort()) || (!is_active_context())) {
        return;
    }

    // TODO Equivalent of: this.connectionToAbort.abortInternal();
}

// Update whether the connection is still valid if the total elapsed time has passed the grace period.
void MONITOR_CONNECTION_CONTEXT::update_connection_status(long status_check_start_time,
                                                        long current_time,
                                                        bool is_valid) {
    if (!is_active_context()) {
      return;
    }

    const long total_elapsed_time_ms = current_time - get_start_monitor_time();

    if (total_elapsed_time_ms > get_failure_detection_time_ms()) {
      set_connection_valid(is_valid, status_check_start_time, current_time);
    }
}

// Set whether the connection to the server is still valid based on the monitoring settings.
void MONITOR_CONNECTION_CONTEXT::set_connection_valid(bool connection_valid,
                                                      long status_check_start_time,
                                                      long current_time) {
    if (!connection_valid) {
        increment_failure_count();

        if (!is_invalid_node_start_time_defined()) {
            set_invalid_node_start_time(status_check_start_time);
        }

        const long invalid_node_duration_ms = current_time - get_invalid_node_start_time();
        const long max_invalid_node_duration_ms = get_failure_detection_interval_ms() * (std::max)(0, get_failure_detection_count());

        if (invalid_node_duration_ms >= max_invalid_node_duration_ms) {
            // TODO MYLOG_TRACE(log_file, 0, "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *dead*.", node_keys);
            set_node_unhealthy(true);
            abort_connection();
            return;
        }

        // TODO MYLOG_TRACE(log_file, 0, "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *not responding* (%d).", node_keys, get_failure_count());
        return;
    }

    set_failure_count(0);
    reset_invalid_node_start_time();
    set_node_unhealthy(false);
    // TODO MYLOG_TRACE(log_file, 0, "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *alive*.", node_keys);
}
