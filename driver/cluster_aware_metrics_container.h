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

#ifndef __CLUSTERAWAREMETRICSCONTAINER_H__
#define __CLUSTERAWAREMETRICSCONTAINER_H__

#include <unordered_map>
#include <functional>

#include "cluster_aware_time_metrics_holder.h"
#include "cluster_aware_metrics.h"
#include "mylog.h"

#define DEFAULT_CLUSTER_ID "no_id"

struct DBC;
struct DataSource;

class CLUSTER_AWARE_METRICS_CONTAINER {
public:
    CLUSTER_AWARE_METRICS_CONTAINER();
    CLUSTER_AWARE_METRICS_CONTAINER(DBC* dbc, DataSource* ds);

    ~CLUSTER_AWARE_METRICS_CONTAINER();

    void set_cluster_id(std::string cluster_id);
    void register_failure_detection_time(long time_ms);
    void register_writer_failover_procedure_time(long time_ms);
    void register_reader_failover_procedure_time(long time_ms);
    void register_failover_connects(bool is_hit);
    void register_invalid_initial_connection(bool is_hit);
    void register_use_cached_topology(bool is_hit);
    void register_topology_query_execution_time(long time_ms);
    
    void set_gather_metric(bool can_gather);

    static void report_metrics(std::string conn_url, bool for_instances, FILE* log, unsigned long dbc_id);
    static std::string report_metrics(std::string conn_url, bool for_instances);
    static void reset_metrics();

private:
    // ClusterID, Metrics
    static std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> cluster_metrics;
    // Instance URL, Metrics
    static std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> instance_metrics;

    bool can_gather = false;    
    DBC* dbc = nullptr;
    DataSource* ds = nullptr;
    std::string cluster_id = DEFAULT_CLUSTER_ID;

protected:
    bool is_enabled();
    bool is_instance_metrics_enabled();

    std::shared_ptr<CLUSTER_AWARE_METRICS> get_cluster_metrics(std::string key);
    std::shared_ptr<CLUSTER_AWARE_METRICS> get_instance_metrics(std::string key);
    virtual std::string get_curr_conn_url();

    void register_metrics(std::function<void(std::shared_ptr<CLUSTER_AWARE_METRICS>)> lambda);
};

#endif /* __CLUSTERAWAREMETRICSCONTAINER_H__ */
