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

#include "failover.h"
#include "mylog.h"

#include <set>

// Monitoring context for each connection. This contains each connection's criteria for
// whether a server should be considered unhealthy.
class MONITOR_CONNECTION_CONTEXT {
public:
    MONITOR_CONNECTION_CONTEXT(DBC* connection_to_abort,
                               std::set<std::string> node_keys,
                               int failure_detection_interval_ms,
                               int failure_detection_time_ms,
                               int failure_detection_count);
    ~MONITOR_CONNECTION_CONTEXT();

    long get_start_monitor_time();
    void set_start_monitor_time(long time);
    std::set<std::string> get_node_keys();
    int get_failure_detection_time_ms();
    int get_failure_detection_interval_ms();
    int get_failure_detection_count();
    int get_failure_count();
    void set_failure_count(int count);
    void increment_failure_count();
    void set_invalid_node_start_time(long time_ms);
    void reset_invalid_node_start_time();
    bool is_invalid_node_start_time_defined();
    long get_invalid_node_start_time();
    bool is_node_unhealthy();
    void set_node_unhealthy(bool node);
    bool is_active_context();
    void invalidate();
    DBC* get_connection_to_abort();

    void update_connection_status(long status_check_start_time, long current_time, bool is_valid);
    void set_connection_valid(bool connection_valid, long status_check_start_time, long current_time);
    void abort_connection();

private:
    int failure_detection_interval_ms;
    int failure_detection_time_ms;
    int failure_detection_count;

    std::set<std::string> node_keys;
    DBC* connection_to_abort;

    long start_monitor_time;
    long invalid_node_start_time;
    int failure_count;
    bool node_unhealthy;
    bool active_context = true;
};

#endif /* __MONITORCONNECTIONCONTEXT_H__ */
