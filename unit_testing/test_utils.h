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

#ifndef __TESTUTILS_H__
#define __TESTUTILS_H__

#include "driver/auth_util.h"
#include "driver/cache_map.h"
#include "driver/custom_endpoint_info.h"
#include "driver/custom_endpoint_monitor.h"
#include "driver/driver.h"
#include "driver/failover.h"
#include "driver/iam_proxy.h"
#include "driver/monitor.h"
#include "driver/monitor_thread_container.h"
#include "driver/okta_proxy.h"
#include "driver/rds_utils.h"
#include "driver/secrets_manager_proxy.h"
#include "driver/sliding_expiration_cache_with_clean_up_thread.h"

void allocate_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds);
void cleanup_odbc_handles(SQLHENV env, DBC*& dbc, DataSource*& ds, bool call_myodbc_end = false);

class TEST_UTILS {
 public:
  static std::chrono::milliseconds get_connection_check_interval(std::shared_ptr<MONITOR> monitor);
  static CONNECTION_STATUS check_connection_status(std::shared_ptr<MONITOR> monitor);
  static void populate_monitor_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::set<std::string> node_keys,
                                   std::shared_ptr<MONITOR> monitor);
  static void populate_task_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::shared_ptr<MONITOR> monitor);
  static bool has_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::string node_key);
  static bool has_task(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::shared_ptr<MONITOR> monitor);
  static bool has_any_tasks(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
  static bool has_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
  static std::shared_ptr<MONITOR> get_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
  static size_t get_map_size(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
  static std::list<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> get_contexts(std::shared_ptr<MONITOR> monitor);
  static std::string build_cache_key(const char* host, const char* region, unsigned int port, const char* user);
  static bool token_cache_contains_key(std::unordered_map<std::string, TOKEN_INFO> token_cache, std::string cache_key);
  static std::map<std::pair<Aws::String, Aws::String>, Aws::Utils::Json::JsonValue>& get_secrets_cache();
  static bool try_parse_region_from_secret(std::string secret, std::string& region);
  static bool is_dns_pattern_valid(std::string host);
  static bool is_rds_dns(std::string host);
  static bool is_rds_cluster_dns(std::string host);
  static bool is_rds_proxy_dns(std::string host);
  static bool is_rds_writer_cluster_dns(std::string host);
  static bool is_rds_custom_cluster_dns(std::string host);
  static std::string get_rds_cluster_host_url(std::string host);
  static std::string get_rds_cluster_id(std::string host);
  static std::string get_rds_instance_id(std::string host);
  static std::string get_rds_instance_host_pattern(std::string host);
  static CACHE_MAP<std::string, std::shared_ptr<CUSTOM_ENDPOINT_INFO>>& get_custom_endpoint_cache();
  static SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<std::string, std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>>&
  get_custom_endpoint_monitor_cache();
};

#endif /* __TESTUTILS_H__ */
