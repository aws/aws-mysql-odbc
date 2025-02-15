// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see 
// http://www.gnu.org/licenses/gpl-2.0.html.

#ifndef __MOCKOBJECTS_H__
#define __MOCKOBJECTS_H__

#include <aws/secretsmanager/SecretsManagerClient.h>
#include <aws/rds/RdsClient.h>
#include <gmock/gmock.h>

#include "driver/connection_proxy.h"
#include "driver/custom_endpoint_proxy.h"
#include "driver/failover.h"
#include "driver/saml_http_client.h"
#include "driver/monitor_thread_container.h"
#include "driver/monitor_service.h"

#ifdef WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#define DEBUG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
#endif

class MOCK_CONNECTION_PROXY : public CONNECTION_PROXY {
public:
    MOCK_CONNECTION_PROXY(DBC* dbc, DataSource* ds) : CONNECTION_PROXY(dbc, ds) {
        this->ds = new DataSource();
        this->ds->copy(ds);
    };
    ~MOCK_CONNECTION_PROXY() override {
        mock_connection_proxy_destructor();
        if (this->ds) {
            delete ds;
            this->ds = nullptr;
        }
    }

    DataSource* get_ds() {
        return this->ds;
    };

    MOCK_METHOD(bool, is_connected, ());
    MOCK_METHOD(std::string, get_host, ());
    MOCK_METHOD(unsigned int, get_port, ());
    MOCK_METHOD(int, query, (const char*));
    MOCK_METHOD(MYSQL_RES*, store_result, ());
    MOCK_METHOD(char**, fetch_row, (MYSQL_RES*));
    MOCK_METHOD(void, free_result, (MYSQL_RES*));
    MOCK_METHOD(void, close_socket, ());
    MOCK_METHOD(void, mock_connection_proxy_destructor, ());
    MOCK_METHOD(void, close, ());
    MOCK_METHOD(void, init, ());
    MOCK_METHOD(int, ping, ());
    MOCK_METHOD(void, delete_ds, ());
    MOCK_METHOD(bool, connect, (const char*, const char*, const char*, const char*, unsigned int, const char*, unsigned long));
    MOCK_METHOD(unsigned int, error_code, ());
};

class MOCK_TOPOLOGY_SERVICE : public TOPOLOGY_SERVICE {
public:
    MOCK_TOPOLOGY_SERVICE() : TOPOLOGY_SERVICE(0) {};

    MOCK_METHOD(void, set_cluster_id, (std::string));
    MOCK_METHOD(void, set_cluster_instance_template, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, get_topology, (CONNECTION_PROXY*, bool));
    MOCK_METHOD(std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, get_filtered_topology, (CONNECTION_PROXY*, bool));
    MOCK_METHOD(void, mark_host_down, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(void, mark_host_up, (std::shared_ptr<HOST_INFO>));
};

class MOCK_READER_HANDLER : public FAILOVER_READER_HANDLER {
public:
    MOCK_READER_HANDLER() : FAILOVER_READER_HANDLER(nullptr, nullptr, thread_pool, 0, 0, false, 0) {}
    MOCK_METHOD(std::shared_ptr<READER_FAILOVER_RESULT>, get_reader_connection,
        (std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, std::shared_ptr<FAILOVER_SYNC>));

    ctpl::thread_pool thread_pool;
};

class MOCK_CONNECTION_HANDLER : public CONNECTION_HANDLER {
public:

    CONNECTION_PROXY* connect(std::shared_ptr<HOST_INFO> host_info, DataSource* ds, bool is_monitor_connection = false) {
        return connect_impl(host_info, ds, is_monitor_connection);
    }

    SQLRETURN do_connect(DBC* dbc_ptr, DataSource* ds, bool failover_enabled, bool is_monitor_connection = false) {
        return do_connect_impl(dbc_ptr, ds, failover_enabled, is_monitor_connection);
    }

    MOCK_CONNECTION_HANDLER() : CONNECTION_HANDLER(nullptr) {}
    MOCK_METHOD(CONNECTION_PROXY*, connect_impl, (std::shared_ptr<HOST_INFO>, DataSource*, bool));
    MOCK_METHOD(SQLRETURN, do_connect_impl, (DBC*, DataSource*, bool, bool));
};

class MOCK_FAILOVER_HANDLER : public FAILOVER_HANDLER {
public:
    MOCK_FAILOVER_HANDLER(DBC* dbc, DataSource* ds, std::shared_ptr<CONNECTION_HANDLER> ch,
        std::shared_ptr<TOPOLOGY_SERVICE> ts, std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> mc) :
        FAILOVER_HANDLER(dbc, ds, ch, ts, mc) {}
    MOCK_METHOD(std::string, host_to_IP, (std::string));
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
    MOCK_MONITOR(std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time,
        CONNECTION_PROXY* monitor_proxy)
        : MONITOR(host, nullptr, std::chrono::seconds{ 5 }, disposal_time, nullptr, monitor_proxy) {}

    MOCK_METHOD(void, start_monitoring, (std::shared_ptr<MONITOR_CONNECTION_CONTEXT>));
    MOCK_METHOD(void, stop_monitoring, (std::shared_ptr<MONITOR_CONNECTION_CONTEXT>));
    MOCK_METHOD(bool, is_stopped, ());
    MOCK_METHOD(void, run, (std::shared_ptr<MONITOR_SERVICE>));
};

// Meant for tests that only need to mock Monitor.run()
class MOCK_MONITOR2 : public MONITOR {
public:
    MOCK_MONITOR2(std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time)
        : MONITOR(host, nullptr, std::chrono::seconds{ 5 }, disposal_time, nullptr, nullptr) {}

    MOCK_METHOD(void, run, (std::shared_ptr<MONITOR_SERVICE>));
};

// Meant for tests that only need to mock get_current_time()
class MOCK_MONITOR3 : public MONITOR {
public:
    MOCK_MONITOR3(std::shared_ptr<HOST_INFO> host,
        std::chrono::milliseconds disposal_time)
        : MONITOR(host, nullptr, std::chrono::seconds{ 5 }, disposal_time, nullptr, nullptr) {}

    MOCK_METHOD(std::chrono::steady_clock::time_point, get_current_time, ());
};

class MOCK_MONITOR_THREAD_CONTAINER : public MONITOR_THREAD_CONTAINER {
public:
    MOCK_MONITOR_THREAD_CONTAINER() : MONITOR_THREAD_CONTAINER() {}

    void release_resources() {
        MONITOR_THREAD_CONTAINER::release_resources();
    }

    MOCK_METHOD(std::shared_ptr<MONITOR>, create_monitor,
        (std::shared_ptr<HOST_INFO>, std::shared_ptr<CONNECTION_HANDLER>, std::chrono::seconds, std::chrono::milliseconds, DataSource*, bool));
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

class MOCK_MONITOR_SERVICE : public MONITOR_SERVICE {
public:
    MOCK_MONITOR_SERVICE() : MONITOR_SERVICE() {};

    MOCK_METHOD(std::shared_ptr<MONITOR_CONNECTION_CONTEXT>, start_monitoring,
        (DBC*, DataSource*, std::set<std::string>,
            std::shared_ptr<HOST_INFO>, std::chrono::milliseconds,
            std::chrono::seconds, std::chrono::milliseconds, int, std::chrono::milliseconds));
    MOCK_METHOD(void, stop_monitoring, (std::shared_ptr<MONITOR_CONNECTION_CONTEXT>));
    MOCK_METHOD(void, stop_monitoring_for_all_connections, (std::set<std::string>));
};

class MOCK_SECRETS_MANAGER_CLIENT : public Aws::SecretsManager::SecretsManagerClient {
public:
    MOCK_SECRETS_MANAGER_CLIENT() : SecretsManagerClient() {};

    MOCK_METHOD(Aws::SecretsManager::Model::GetSecretValueOutcome, GetSecretValue, (const Aws::SecretsManager::Model::GetSecretValueRequest&), (const));
};

class MOCK_RDS_CLIENT : public Aws::RDS::RDSClient {
public:
    MOCK_RDS_CLIENT() : RDSClient(){};

    MOCK_METHOD(Aws::RDS::Model::DescribeDBClusterEndpointsOutcome, DescribeDBClusterEndpoints, (const Aws::RDS::Model::DescribeDBClusterEndpointsRequest&), (const));
};

class MOCK_AUTH_UTIL : public AUTH_UTIL {
public:
    MOCK_AUTH_UTIL() : AUTH_UTIL() {};
    MOCK_METHOD(std::string, generate_token, (const char*, const char*, unsigned int, const char*));
};

class MOCK_SAML_HTTP_CLIENT : public SAML_HTTP_CLIENT {
public:
    MOCK_SAML_HTTP_CLIENT(std::string host, int connect_timeout, int socket_timeout, bool enable_ssl) : SAML_HTTP_CLIENT(host, connect_timeout, socket_timeout, enable_ssl) {};
    MOCK_METHOD(nlohmann::json, post, (const std::string&, const std::string&, const std::string&));
    MOCK_METHOD(nlohmann::json, get, (const std::string&, const httplib::Headers&));
};

class TEST_CUSTOM_ENDPOINT_PROXY : public CUSTOM_ENDPOINT_PROXY {
public:
    TEST_CUSTOM_ENDPOINT_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CUSTOM_ENDPOINT_PROXY(dbc, ds, next_proxy) {};
    MOCK_METHOD(std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>, create_custom_endpoint_monitor, (const long long refresh_rate_nanos), (override));
    static int get_monitor_size() { return monitors.size(); }
};

class MOCK_CUSTOM_ENDPOINT_MONITOR : public CUSTOM_ENDPOINT_MONITOR {
 public:
  MOCK_CUSTOM_ENDPOINT_MONITOR(ctpl::thread_pool& pool) : CUSTOM_ENDPOINT_MONITOR(pool) {};
};
#endif /* __MOCKOBJECTS_H__ */
