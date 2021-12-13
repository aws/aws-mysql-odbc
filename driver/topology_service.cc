// Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an
// additional permission to link the program and your derivative works
// with the separately licensed software that they have included with
// MySQL.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of <MySQL Product>, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

/**
  @file  topology_service.cc
  @brief TOPOLOGY_SERVICE functions.
*/

#include "topology_service.h"

TOPOLOGY_SERVICE::TOPOLOGY_SERVICE()
    : cluster_instance_host{ nullptr }, refresh_rate_in_milliseconds{ DEFAULT_REFRESH_RATE_IN_MILLISECONDS }
{
    //TODO get better initial cluster id
    time_t now = time(0);
    cluster_id = std::to_string(now) + ctime(&now);
}

void TOPOLOGY_SERVICE::set_cluster_id(const char* cluster_id) {
    this->cluster_id = cluster_id;
}

void TOPOLOGY_SERVICE::set_cluster_instance_template(std::shared_ptr<HOST_INFO> host_template) {

    // NOTE, this may not have to be a pointer. Copy the information passed to this function.
    // Alernatively the create host function should be part of the Topologyservice, even protected or private one
    // and host information passed as separate parameters.
    if (cluster_instance_host)
        cluster_instance_host.reset();

    cluster_instance_host = host_template;

}

void TOPOLOGY_SERVICE::set_refresh_rate(int refresh_rate) {
    refresh_rate_in_milliseconds = refresh_rate;
}

std::shared_ptr<HOST_INFO> TOPOLOGY_SERVICE::get_last_used_reader() {
    auto topology_info = get_from_cache();
    if (!topology_info || refresh_needed(topology_info->time_last_updated())) {
        return nullptr;
    }

    return topology_info->get_last_used_reader();
}

void TOPOLOGY_SERVICE::set_last_used_reader(std::shared_ptr<HOST_INFO> reader) {
    if (reader) {
        std::unique_lock<std::mutex> lock(topology_cache_mutex);
        auto topology_info = get_from_cache();
        if (topology_info) {
            topology_info->set_last_used_reader(reader);
        }
        lock.unlock();
    }
}

std::set<std::string> TOPOLOGY_SERVICE::get_down_hosts() {
    std::set<std::string> down_hosts;

    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    auto topology_info = get_from_cache();
    if (topology_info) {
        down_hosts = topology_info->get_down_hosts();
    }
    lock.unlock();

    return down_hosts;
}

void TOPOLOGY_SERVICE::mark_host_down(std::shared_ptr<HOST_INFO> down_host) {
    if (!down_host) {
        return;
    }

    std::unique_lock<std::mutex> lock(topology_cache_mutex);

    auto topology_info = get_from_cache();
    if (topology_info) {
        topology_info->mark_host_down(down_host);
    }

    lock.unlock();
}

void TOPOLOGY_SERVICE::unmark_host_down(std::shared_ptr<HOST_INFO> host) {
    if (!host) {
        return;
    }

    std::unique_lock<std::mutex> lock(topology_cache_mutex);

    auto topology_info = get_from_cache();
    if (topology_info) {
        topology_info->unmark_host_down(host);
    }

    lock.unlock();
}

void TOPOLOGY_SERVICE::clear_all() {
    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    topology_cache.clear();
    lock.unlock();
}

void TOPOLOGY_SERVICE::clear() {
    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    topology_cache.erase(cluster_id);
    lock.unlock();
}

TOPOLOGY_SERVICE::~TOPOLOGY_SERVICE() {
    if (cluster_instance_host)
        cluster_instance_host.reset();

    for (auto p : topology_cache) {
        p.second.reset();
    }
    topology_cache.clear();
}

std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::get_cached_topology() {
    return get_from_cache();
}

//TODO consider the return value
//Note to determine whether or not force_update succeeded one would compare
// CLUSTER_TOPOLOGY_INFO->time_last_updated() prior and after the call if non-null information was given prior.
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::get_topology(std::shared_ptr<MYSQL> connection, bool force_update)
{
    //TODO reconsider using this cache. It appears that we only store information for the current cluster Id.
    // therefore instead of a map we can just keep CLUSTER_TOPOLOGY_INFO* topology_info member variable.
    auto cached_topology = get_from_cache();
    if (!cached_topology
        || force_update
        || refresh_needed(cached_topology->time_last_updated()))
    {
        auto latest_topology = query_for_topology(connection);
        if (latest_topology) {
            put_to_cache(latest_topology);
            return latest_topology;
        }
    }

    return cached_topology;
}

// TODO consider thread safety and usage of pointers
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::get_from_cache() {
    if (topology_cache.empty())
        return nullptr;
    auto result = topology_cache.find(cluster_id);
    return result != topology_cache.end() ? result->second : nullptr;
}

// TODO consider thread safety and usage of pointers
void TOPOLOGY_SERVICE::put_to_cache(std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info) {
    if (!topology_cache.empty())
    {
        auto result = topology_cache.find(cluster_id);
        if (result != topology_cache.end()) {
            result->second.reset();
            result->second = topology_info;
            return;
        }
    }
    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    topology_cache[cluster_id] = topology_info;
    lock.unlock();
}

// TODO harmonize time function accross objects so the times are comparable
bool TOPOLOGY_SERVICE::refresh_needed(std::time_t last_updated) {

    return  time(0) - last_updated > (refresh_rate_in_milliseconds / 1000);
}

std::shared_ptr<HOST_INFO> TOPOLOGY_SERVICE::create_host(MYSQL_ROW& row) {

    //TEMP and TODO figure out how to fetch values from row by name, not by ordinal for now this enum is matching
    // order of columns in the query
    enum COLUMNS {
        SERVER_ID,
        SESSION,
        LAST_UPDATE_TIMESTAMP,
        REPLICA_LAG_MILLISECONDS
    };

    if (row[SERVER_ID] == NULL) {
        return nullptr;  // will not be able to generate host endpoint so no point. TODO: log this condition?
    }

    std::string host_endpoint = get_host_endpoint(row[SERVER_ID]);

    //TODO check cluster_instance_host for NULL, or decide what is needed out of it
    std::shared_ptr<HOST_INFO> host_info = std::make_shared<HOST_INFO>(
        HOST_INFO(
            host_endpoint,
            cluster_instance_host->get_port()
        ));

    //TODO do case-insensitive comparison
    // side note: how stable this is on the server side? If it changes there we will not detect a writter.
    if (WRITER_SESSION_ID == row[SESSION])
    {
        host_info->mark_as_writer(true);
    }

    host_info->instance_name = row[SERVER_ID] ? row[SERVER_ID] : "";
    host_info->session_id = row[SESSION] ? row[SESSION] : "";
    host_info->last_updated = row[LAST_UPDATE_TIMESTAMP] ? row[LAST_UPDATE_TIMESTAMP] : "";
    host_info->replica_lag = row[REPLICA_LAG_MILLISECONDS] ? row[REPLICA_LAG_MILLISECONDS] : "";

    return host_info;
}

// If no host information retrieved return NULL
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::query_for_topology(std::shared_ptr<MYSQL> connection) {

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info = nullptr;

    MYSQL* conn = connection.get();
    MYSQL_RES* query_result = NULL;
    if ((conn != NULL) && (mysql_query(conn, RETRIEVE_TOPOLOGY_SQL) == 0) && (query_result = mysql_store_result(conn)) != NULL) {
        topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(CLUSTER_TOPOLOGY_INFO());
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(query_result))) {
            std::shared_ptr<HOST_INFO> host_info = create_host(row);
            if (!host_info) {
                topology_info->add_host(host_info);
            }
        }
        mysql_free_result(query_result);

        if (topology_info->total_hosts() == 0) {
            topology_info.reset();
        }
    }
    return topology_info;
}

std::string TOPOLOGY_SERVICE::get_host_endpoint(const char* node_name) {
    std::string host = cluster_instance_host->get_host();
    size_t position = host.find("?");
    if (position != std::string::npos) {
        host.replace(position, 1, node_name);
    }
    else {
        throw "Invalid host template for TOPOLOGY_SERVICE";
    }

    return host;
}
