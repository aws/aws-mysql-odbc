/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __MOCKOBJECTS_H__
#define __MOCKOBJECTS_H__

#include <gmock/gmock.h>

#include "driver/mysql_proxy.h"
#include "driver/failover.h"
#include "driver/monitor_thread_container.h"

#ifdef WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#define DEBUG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
#endif

class MOCK_MYSQL_PROXY : public MYSQL_PROXY {
 public:
    MOCK_MYSQL_PROXY(DBC* dbc, DataSource* ds) : MYSQL_PROXY(dbc, ds) {};
    ~MOCK_MYSQL_PROXY() { mock_mysql_proxy_destructor(); }

    MOCK_METHOD(bool, is_connected, ());
    MOCK_METHOD(std::string, get_host, ());
    MOCK_METHOD(unsigned int, get_port, ());
    MOCK_METHOD(int, query, (const char*));
    MOCK_METHOD(MYSQL_RES*, store_result, ());
    MOCK_METHOD(char**, fetch_row, (MYSQL_RES*));
    MOCK_METHOD(void, free_result, (MYSQL_RES*));
    MOCK_METHOD(void, close_socket, ());
    MOCK_METHOD(void, mock_mysql_proxy_destructor, ());
};

class MOCK_TOPOLOGY_SERVICE : public TOPOLOGY_SERVICE {
 public:
    MOCK_TOPOLOGY_SERVICE() : TOPOLOGY_SERVICE(0) {};

    MOCK_METHOD(void, set_cluster_id, (std::string));
    MOCK_METHOD(void, set_cluster_instance_template, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, get_topology, (MYSQL_PROXY*, bool));
    MOCK_METHOD(void, mark_host_down, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(void, mark_host_up, (std::shared_ptr<HOST_INFO>));
};

class MOCK_READER_HANDLER : public FAILOVER_READER_HANDLER {
 public:
    MOCK_READER_HANDLER() : FAILOVER_READER_HANDLER(nullptr, nullptr, 0, 0, 0) {}
    MOCK_METHOD(READER_FAILOVER_RESULT, get_reader_connection,
                (std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, FAILOVER_SYNC&));
};

class MOCK_CONNECTION_HANDLER : public FAILOVER_CONNECTION_HANDLER {
 public:
    MOCK_CONNECTION_HANDLER() : FAILOVER_CONNECTION_HANDLER(nullptr) {}
    MOCK_METHOD(MYSQL_PROXY*, connect, (const std::shared_ptr<HOST_INFO>&));
    MOCK_METHOD(SQLRETURN, do_connect, (DBC*, DataSource*, bool));
};

class MOCK_FAILOVER_SYNC : public FAILOVER_SYNC {
public:
    MOCK_FAILOVER_SYNC() : FAILOVER_SYNC(1) {}
    MOCK_METHOD(bool, is_completed, ());
};

class MOCK_CLUSTER_AWARE_METRICS_CONTAINER : public CLUSTER_AWARE_METRICS_CONTAINER {
public:
    MOCK_CLUSTER_AWARE_METRICS_CONTAINER() : CLUSTER_AWARE_METRICS_CONTAINER() {}
    MOCK_CLUSTER_AWARE_METRICS_CONTAINER(DBC* dbc, DataSource* ds) : CLUSTER_AWARE_METRICS_CONTAINER(dbc, ds) {}

    // Expose protected functions
    bool is_enabled() { return CLUSTER_AWARE_METRICS_CONTAINER::is_enabled(); }
    bool is_instance_metrics_enabled() { return CLUSTER_AWARE_METRICS_CONTAINER::is_instance_metrics_enabled(); }

    MOCK_METHOD(std::string, get_curr_conn_url, ());
};

class MOCK_MONITOR : public MONITOR {
public:
    MOCK_MONITOR(std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time, MONITOR_SERVICE* service) : 
        MONITOR(host, disposal_time, (MYSQL_MONITOR_PROXY*)nullptr, service) {}
    
    MOCK_METHOD(void, start_monitoring, (std::shared_ptr<MONITOR_CONNECTION_CONTEXT>));
    MOCK_METHOD(void, stop_monitoring, (std::shared_ptr<MONITOR_CONNECTION_CONTEXT>));
    MOCK_METHOD(bool, is_stopped, ());
    MOCK_METHOD(void, run, ());
};

// Meant for tests that only need to mock Monitor.run()
class MOCK_MONITOR2 : public MONITOR {
public:
    MOCK_MONITOR2(std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time, MONITOR_SERVICE* service) :
        MONITOR(host, disposal_time, (MYSQL_MONITOR_PROXY*)nullptr, service) {}

    MOCK_METHOD(void, run, ());
};

// Meant for tests that only need to mock get_current_time()
class MOCK_MONITOR3 : public MONITOR {
public:
    MOCK_MONITOR3(std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time, MONITOR_SERVICE* service) :
        MONITOR(host, disposal_time, (MYSQL_MONITOR_PROXY*)nullptr, service) {}

    MOCK_METHOD(std::chrono::steady_clock::time_point, get_current_time, ());
};

class MOCK_MONITOR_THREAD_CONTAINER : public MONITOR_THREAD_CONTAINER {
public:
    MOCK_MONITOR_THREAD_CONTAINER() : MONITOR_THREAD_CONTAINER() {}
    MOCK_METHOD(std::shared_ptr<MONITOR>, create_monitor, 
        (std::shared_ptr<HOST_INFO>, std::chrono::milliseconds, DataSource*, MONITOR_SERVICE*, bool));
};

class MOCK_MONITOR_CONNECTION_CONTEXT : public MONITOR_CONNECTION_CONTEXT {
public:
    MOCK_MONITOR_CONNECTION_CONTEXT(
        std::set<std::string> node_keys,
        std::chrono::milliseconds failure_detection_time,
        std::chrono::milliseconds failure_detection_interval,
        int failure_detection_count) : 
            MONITOR_CONNECTION_CONTEXT(
                nullptr, node_keys, failure_detection_time, failure_detection_interval, failure_detection_count) {}

    MOCK_METHOD(void, set_start_monitor_time, (std::chrono::steady_clock::time_point));
};

class MOCK_MYSQL_MONITOR_PROXY : public MYSQL_MONITOR_PROXY {
public:
    MOCK_MYSQL_MONITOR_PROXY() : MYSQL_MONITOR_PROXY(nullptr) {};

    MOCK_METHOD(bool, is_connected, ());
    MOCK_METHOD(void, init, ());
    MOCK_METHOD(int, ping, ());
    MOCK_METHOD(int, options, (enum mysql_option, const void*));
    MOCK_METHOD(bool, connect, ());
};

class MOCK_MONITOR_SERVICE : public MONITOR_SERVICE {
public:
    MOCK_MONITOR_SERVICE() : MONITOR_SERVICE() {};

    MOCK_METHOD(std::shared_ptr<MONITOR_CONNECTION_CONTEXT>, start_monitoring,
                (DBC*, std::set<std::string>, std::shared_ptr<HOST_INFO>, std::chrono::milliseconds, std::chrono::
                    milliseconds, int, std::chrono::milliseconds));
    MOCK_METHOD(void, stop_monitoring, (std::shared_ptr<MONITOR_CONNECTION_CONTEXT>));
    MOCK_METHOD(void, stop_monitoring_for_all_connections, (std::set<std::string>));
};

#endif /* __MOCKOBJECTS_H__ */
