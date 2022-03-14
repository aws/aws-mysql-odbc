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

#include "cluster_aware_metrics_container.h"
#include "driver.h"
#include "installer.h"

std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> CLUSTER_AWARE_METRICS_CONTAINER::cluster_metrics = {};
std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> CLUSTER_AWARE_METRICS_CONTAINER::instance_metrics = {};

CLUSTER_AWARE_METRICS_CONTAINER::CLUSTER_AWARE_METRICS_CONTAINER() {}

CLUSTER_AWARE_METRICS_CONTAINER::CLUSTER_AWARE_METRICS_CONTAINER(DBC* dbc, DataSource* ds) {
    this->dbc = dbc;
    this->ds = ds;
}

CLUSTER_AWARE_METRICS_CONTAINER::~CLUSTER_AWARE_METRICS_CONTAINER() {}

void CLUSTER_AWARE_METRICS_CONTAINER::set_cluster_id(std::string id) {
    this->cluster_id = id;
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_failure_detection_time(long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_failure_detection_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_writer_failover_procedure_time(long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_writer_failover_procedure_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_reader_failover_procedure_time(long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_reader_failover_procedure_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_failover_connects(bool is_hit) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([is_hit](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_failover_connects(is_hit);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_invalid_initial_connection(bool is_hit) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([is_hit](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_invalid_initial_connection(is_hit);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_use_cached_topology(bool is_hit) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([is_hit](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_use_cached_topology(is_hit);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_topology_query_execution_time(long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_topology_query_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::set_gather_metric(bool can_gather) {
    this->can_gather = can_gather;
}

void CLUSTER_AWARE_METRICS_CONTAINER::report_metrics(std::string conn_url, bool for_instances, FILE* log) {
    if (!log) {
        return;
    }
    
    MYLOG_TRACE(log, report_metrics(conn_url, for_instances).c_str());
}

std::string CLUSTER_AWARE_METRICS_CONTAINER::report_metrics(std::string conn_url, bool for_instances) {
    std::string log_message = "";

    std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>>::const_iterator has_metrics = for_instances ? instance_metrics.find(conn_url) : cluster_metrics.find(conn_url);
    if ((for_instances ? instance_metrics.end() : cluster_metrics.end()) == has_metrics) {
        log_message.append("** No metrics collected for '")
            .append(conn_url)
            .append("' **\n");

        return log_message;
    }

    std::shared_ptr<CLUSTER_AWARE_METRICS> metrics = has_metrics->second;

    log_message.append("** Performance Metrics Report for '")
        .append(conn_url)
        .append("' **\n");

    log_message.append(metrics->report_metrics());

    return log_message;
}

void CLUSTER_AWARE_METRICS_CONTAINER::reset_metrics() {
    cluster_metrics.clear();
    instance_metrics.clear();
}

bool CLUSTER_AWARE_METRICS_CONTAINER::is_enabled() {
    if (ds) {
        return ds->gather_perf_metrics;
    }
    return can_gather;
}

bool CLUSTER_AWARE_METRICS_CONTAINER::is_instance_metrics_enabled() {
    if (ds) {
        return ds->gather_metrics_per_instance;
    }
    return can_gather;
}

std::shared_ptr<CLUSTER_AWARE_METRICS> CLUSTER_AWARE_METRICS_CONTAINER::get_cluster_metrics(std::string key) {
    cluster_metrics.emplace(key, std::make_shared<CLUSTER_AWARE_METRICS>());
    return cluster_metrics.at(key);
}

std::shared_ptr<CLUSTER_AWARE_METRICS> CLUSTER_AWARE_METRICS_CONTAINER::get_instance_metrics(std::string key) {
    instance_metrics.emplace(key, std::make_shared<CLUSTER_AWARE_METRICS>());
    return instance_metrics.at(key);
}

std::string CLUSTER_AWARE_METRICS_CONTAINER::get_curr_conn_url() {
    std::string curr_url = "[Unknown Url]";
    if (dbc && dbc->mysql) {
        curr_url = dbc->mysql->get_host();
        curr_url.append(":").append(std::to_string(dbc->mysql->get_port()));
    }
    return curr_url;
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_metrics(std::function<void(std::shared_ptr<CLUSTER_AWARE_METRICS>)> lambda) {
    if (!is_enabled()) {
        return;
    }

    lambda(get_cluster_metrics(this->cluster_id));

    if (is_instance_metrics_enabled()) {
        lambda(get_instance_metrics(get_curr_conn_url()));
    }
}
