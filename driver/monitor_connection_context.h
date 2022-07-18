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

#ifndef __MONITORCONNECTIONCONTEXT_H__
#define __MONITORCONNECTIONCONTEXT_H__

#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <string>

struct DBC;

// Monitoring context for each connection. This contains each connection's criteria for
// whether a server should be considered unhealthy.
class MONITOR_CONNECTION_CONTEXT {
public:
    MONITOR_CONNECTION_CONTEXT(DBC* connection_to_abort,
                               std::set<std::string> node_keys,
                               std::chrono::milliseconds failure_detection_time,
                               std::chrono::milliseconds failure_detection_interval,
                               int failure_detection_count);
    virtual ~MONITOR_CONNECTION_CONTEXT();

    std::chrono::steady_clock::time_point get_start_monitor_time();
    virtual void set_start_monitor_time(std::chrono::steady_clock::time_point time);
    std::set<std::string> get_node_keys();
    std::chrono::milliseconds get_failure_detection_time();
    std::chrono::milliseconds get_failure_detection_interval();
    int get_failure_detection_count();
    int get_failure_count();
    void set_failure_count(int count);
    void increment_failure_count();
    void set_invalid_node_start_time(std::chrono::steady_clock::time_point time);
    void reset_invalid_node_start_time();
    bool is_invalid_node_start_time_defined();
    std::chrono::steady_clock::time_point get_invalid_node_start_time();
    bool is_node_unhealthy();
    void set_node_unhealthy(bool node);
    bool is_active_context();
    void invalidate();
    DBC* get_connection_to_abort();
    unsigned long get_dbc_id();

    void update_connection_status(
        std::chrono::steady_clock::time_point status_check_start_time,
        std::chrono::steady_clock::time_point current_time,
        bool is_valid);
    void set_connection_valid(
        bool connection_valid,
        std::chrono::steady_clock::time_point status_check_start_time,
        std::chrono::steady_clock::time_point current_time);
    void abort_connection();

private:
    std::mutex mutex_;
    
    std::chrono::milliseconds failure_detection_time;
    std::chrono::milliseconds failure_detection_interval;
    int failure_detection_count;

    std::set<std::string> node_keys;
    DBC* connection_to_abort;

    std::chrono::steady_clock::time_point start_monitor_time;
    std::chrono::steady_clock::time_point invalid_node_start_time;
    int failure_count;
    bool node_unhealthy;
    bool active_context = true;
    std::shared_ptr<FILE> logger;

    std::string build_node_keys_str();
};

#endif /* __MONITORCONNECTIONCONTEXT_H__ */
