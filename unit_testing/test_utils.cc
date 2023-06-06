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

#include "test_utils.h"

void allocate_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds) {
    SQLHDBC hdbc = nullptr;

    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
    dbc = static_cast<DBC*>(hdbc);
    ds = ds_new();
}

void cleanup_odbc_handles(SQLHENV env, DBC*& dbc, DataSource*& ds, bool call_myodbc_end) {
    SQLHDBC hdbc = static_cast<SQLHDBC>(dbc);
    if (nullptr != hdbc) {
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    }
    if (nullptr != env) {
        SQLFreeHandle(SQL_HANDLE_ENV, env);
#ifndef _UNIX_
        // Needed to free memory on Windows
        if (call_myodbc_end)
            myodbc_end();
#endif
    }
    if (nullptr != dbc) {
        dbc = nullptr;
    }
    if (nullptr != ds) {
        ds_delete(ds);
        ds = nullptr;
    }
}

std::chrono::milliseconds TEST_UTILS::get_connection_check_interval(std::shared_ptr<MONITOR> monitor) {
    return monitor->get_connection_check_interval();
}

CONNECTION_STATUS TEST_UTILS::check_connection_status(std::shared_ptr<MONITOR> monitor) {
    return monitor->check_connection_status();
}

void TEST_UTILS::populate_monitor_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container,
    std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor) {

    container->populate_monitor_map(node_keys, monitor);
}

void TEST_UTILS::populate_task_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container,
    std::shared_ptr<MONITOR> monitor) {

    container->task_map[monitor] = std::async(std::launch::async, []() {});
}

bool TEST_UTILS::has_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::string node_key) {
    return container->monitor_map.count(node_key) > 0;
}

bool TEST_UTILS::has_task(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::shared_ptr<MONITOR> monitor) {
    return container->task_map.count(monitor) > 0;
}

bool TEST_UTILS::has_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container) {
    return !container->available_monitors.empty();
}

std::shared_ptr<MONITOR> TEST_UTILS::get_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container) {
    if (container->available_monitors.empty()) {
        return nullptr;
    }
    return container->available_monitors.front();
}

bool TEST_UTILS::has_any_tasks(std::shared_ptr<MONITOR_THREAD_CONTAINER> container) {
    return !container->task_map.empty();
}

size_t TEST_UTILS::get_map_size(std::shared_ptr<MONITOR_THREAD_CONTAINER> container) {
    return container->monitor_map.size();
}

std::list<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> TEST_UTILS::get_contexts(std::shared_ptr<MONITOR> monitor) {
    return monitor->contexts;
}

std::string TEST_UTILS::build_cache_key(const char* host, const char* region, unsigned int port, const char* user) {
    return IAM_PROXY::build_cache_key(host, region, port, user);
}

bool TEST_UTILS::token_cache_contains_key(std::string cache_key) {
    return IAM_PROXY::token_cache.find(cache_key) != IAM_PROXY::token_cache.end();
}

void TEST_UTILS::clear_token_cache(IAM_PROXY &iam_proxy) {
    iam_proxy.clear_token_cache();
}

std::map<std::pair<Aws::String, Aws::String>, Aws::Utils::Json::JsonValue>& TEST_UTILS::get_secrets_cache() {
    return std::ref(SECRETS_MANAGER_PROXY::secrets_cache);
}

bool TEST_UTILS::try_parse_region_from_secret(std::string secret, std::string& region) {
    return SECRETS_MANAGER_PROXY::try_parse_region_from_secret(secret, region);
}

bool TEST_UTILS::is_dns_pattern_valid(std::string host) {
    return FAILOVER_HANDLER::is_dns_pattern_valid(host);
}

bool TEST_UTILS::is_rds_dns(std::string host) {
    return FAILOVER_HANDLER::is_rds_dns(host);
}

bool TEST_UTILS::is_rds_cluster_dns(std::string host) {
    return FAILOVER_HANDLER::is_rds_cluster_dns(host);
}

bool TEST_UTILS::is_rds_proxy_dns(std::string host) {
    return FAILOVER_HANDLER::is_rds_proxy_dns(host);
}

bool TEST_UTILS::is_rds_writer_cluster_dns(std::string host) {
    return FAILOVER_HANDLER::is_rds_writer_cluster_dns(host);
}

bool TEST_UTILS::is_rds_custom_cluster_dns(std::string host) {
    return FAILOVER_HANDLER::is_rds_custom_cluster_dns(host);
}

std::string TEST_UTILS::get_rds_cluster_host_url(std::string host) {
    return FAILOVER_HANDLER::get_rds_cluster_host_url(host);
}

std::string TEST_UTILS::get_rds_instance_host_pattern(std::string host) {
    return FAILOVER_HANDLER::get_rds_instance_host_pattern(host);
}
