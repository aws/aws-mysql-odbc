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

#ifndef __TOPOLOGYSERVICE_H__
#define __TOPOLOGYSERVICE_H__

#include "cluster_aware_metrics_container.h"
#include "cluster_topology_info.h"
#include "connection.h"
#include "mylog.h"

#include <map>
#include <mutex>
#include <cstring>
#include <chrono>
#include <ctime>

// TODO - consider - do we really need miliseconds for refresh? - the default numbers here are already 30 seconds.000;
#define DEFAULT_REFRESH_RATE_IN_MILLISECONDS 30000
#define WRITER_SESSION_ID "MASTER_SESSION_ID"
#define RETRIEVE_TOPOLOGY_SQL "SELECT SERVER_ID, SESSION_ID, LAST_UPDATE_TIMESTAMP, REPLICA_LAG_IN_MILLISECONDS \
    FROM information_schema.replica_host_status \
    WHERE time_to_sec(timediff(now(), LAST_UPDATE_TIMESTAMP)) <= 300 \
    ORDER BY LAST_UPDATE_TIMESTAMP DESC"

static std::map<std::string, std::shared_ptr<CLUSTER_TOPOLOGY_INFO>> topology_cache;
static std::mutex topology_cache_mutex;

class TOPOLOGY_SERVICE_INTERFACE {
public:
    virtual ~TOPOLOGY_SERVICE_INTERFACE() {};

    virtual void set_cluster_id(std::string cluster_id) = 0;
    virtual void set_cluster_instance_template(std::shared_ptr<HOST_INFO> host_template) = 0;
    virtual std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_topology(CONNECTION_INTERFACE* connection, bool force_update = false) = 0;
    virtual void mark_host_down(std::shared_ptr<HOST_INFO> host) = 0;
    virtual void mark_host_up(std::shared_ptr<HOST_INFO> host) = 0;
    virtual void set_refresh_rate(int refresh_rate) = 0;
    virtual void set_gather_metric(bool can_gather) = 0;
};

class TOPOLOGY_SERVICE : virtual public TOPOLOGY_SERVICE_INTERFACE {
public:
    TOPOLOGY_SERVICE(FILE* log_file, unsigned long dbc_id);
    TOPOLOGY_SERVICE(const TOPOLOGY_SERVICE&);
    ~TOPOLOGY_SERVICE() override;

    void set_cluster_id(std::string cluster_id) override;
    void set_cluster_instance_template(std::shared_ptr<HOST_INFO> host_template) override;  //is this equivalent to setcluster_instance_host

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_topology(CONNECTION_INTERFACE* connection, bool force_update = false) override;
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_cached_topology();

    std::shared_ptr<HOST_INFO> get_last_used_reader();
    void set_last_used_reader(std::shared_ptr<HOST_INFO> reader);
    std::set<std::string> get_down_hosts();
    void mark_host_down(std::shared_ptr<HOST_INFO> host) override;
    void mark_host_up(std::shared_ptr<HOST_INFO> host) override;
    void set_refresh_rate(int refresh_rate) override;
    void set_gather_metric(bool can_gather) override;
    void clear_all();
    void clear();

    // Property Keys
    const std::string SESSION_ID = "TOPOLOGY_SERVICE_SESSION_ID";
    const std::string LAST_UPDATED = "TOPOLOGY_SERVICE_LAST_UPDATE_TIMESTAMP";
    const std::string REPLICA_LAG = "TOPOLOGY_SERVICE_REPLICA_LAG_IN_MILLISECONDS";
    const std::string INSTANCE_NAME = "TOPOLOGY_SERVICE_SERVER_ID";

private:
    const int DEFAULT_CACHE_EXPIRE_MS = 5 * 60 * 1000; // 5 min

    const std::string GET_INSTANCE_NAME_SQL = "SELECT @@aurora_server_id";
    const std::string GET_INSTANCE_NAME_COL = "@@aurora_server_id";

    const std::string FIELD_SERVER_ID = "SERVER_ID";
    const std::string FIELD_SESSION_ID = "SESSION_ID";
    const std::string FIELD_LAST_UPDATED = "LAST_UPDATE_TIMESTAMP";
    const std::string FIELD_REPLICA_LAG = "REPLICA_LAG_IN_MILLISECONDS";

    FILE* log_file = nullptr;
    unsigned long dbc_id = 0;

protected:
    const int NO_CONNECTION_INDEX = -1;
    int refresh_rate_in_ms;

    std::string cluster_id;
    std::shared_ptr<HOST_INFO> cluster_instance_host;

    std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> metrics_container;

    bool refresh_needed(std::time_t last_updated);
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> query_for_topology(CONNECTION_INTERFACE* connection);
    std::shared_ptr<HOST_INFO> create_host(MYSQL_ROW& row);
    std::string get_host_endpoint(const char* node_name);
    static bool does_instance_exist(
        std::map<std::string, std::shared_ptr<HOST_INFO>>& instances,
        std::shared_ptr<HOST_INFO> host_info);

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_from_cache();
    void put_to_cache(std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info);
};

#endif /* __TOPOLOGYSERVICE_H__ */
