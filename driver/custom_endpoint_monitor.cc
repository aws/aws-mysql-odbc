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

#include "custom_endpoint_monitor.h"

#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include "allowed_and_blocked_hosts.h"
#include "aws_sdk_helper.h"
#include "driver.h"
#include "monitor_service.h"
#include "mylog.h"

namespace {
AWS_SDK_HELPER SDK_HELPER;
}

CACHE_MAP<std::string, std::shared_ptr<CUSTOM_ENDPOINT_INFO>> CUSTOM_ENDPOINT_MONITOR::custom_endpoint_cache;

CUSTOM_ENDPOINT_MONITOR::CUSTOM_ENDPOINT_MONITOR(const std::shared_ptr<HOST_INFO>& custom_endpoint_host_info,
                                                 const std::string& endpoint_identifier, const std::string& region,
                                                 DataSource* ds, bool enable_logging)
  : custom_endpoint_host_info(custom_endpoint_host_info),
    endpoint_identifier(endpoint_identifier),
    region(region),
    enable_logging(enable_logging) {
  if (enable_logging) {
    this->logger = init_log_file();
  }

  ++SDK_HELPER;

  Aws::RDS::RDSClientConfiguration client_config;
  if (!region.empty()) {
    client_config.region = region;
  }

  this->rds_client = std::make_shared<Aws::RDS::RDSClient>(
    Aws::Auth::DefaultAWSCredentialsProviderChain().GetAWSCredentials(), client_config);

  this->run();
}

bool CUSTOM_ENDPOINT_MONITOR::should_dispose() { return true; }

bool CUSTOM_ENDPOINT_MONITOR::has_custom_endpoint_info() const {
  auto default_val = std::shared_ptr<CUSTOM_ENDPOINT_INFO>(nullptr);
  return custom_endpoint_cache.get(this->custom_endpoint_host_info->get_host(), default_val) != default_val;
}

void CUSTOM_ENDPOINT_MONITOR::run() {
  this->thread_pool.resize(1);
  this->thread_pool.push([=](int id) {
    MYLOG_TRACE(this->logger, 0, "Starting custom endpoint monitor for '%s'",
                this->custom_endpoint_host_info->get_host().c_str());

    try {
      while (!this->should_stop.load()) {
        const std::chrono::time_point start = std::chrono::steady_clock::now();
        Aws::RDS::Model::Filter filter;
        filter.SetName("db-cluster-endpoint-type");
        filter.SetValues({"custom"});

        Aws::RDS::Model::DescribeDBClusterEndpointsRequest request;
        request.SetDBClusterIdentifier(this->endpoint_identifier);
        request.SetFilters({filter});
        const auto response = this->rds_client->DescribeDBClusterEndpoints(request);

        const auto custom_endpoints = response.GetResult().GetDBClusterEndpoints();
        if (custom_endpoints.size() != 1) {
          MYLOG_TRACE(this->logger, 0,
                      "Unexpected number of custom endpoints with endpoint identifier '%s' in region '%s'. Expected 1 "
                      "custom endpoint, but found %d. Endpoints: %s",
                      endpoint_identifier.c_str(), region.c_str(), custom_endpoints.size(),
                      this->get_endpoints_as_string(custom_endpoints).c_str());

          std::this_thread::sleep_for(std::chrono::nanoseconds(this->refresh_rate_nanos));
          continue;
        }
        const std::shared_ptr<CUSTOM_ENDPOINT_INFO> endpoint_info =
          CUSTOM_ENDPOINT_INFO::from_db_cluster_endpoint(custom_endpoints[0]);
        const std::shared_ptr<CUSTOM_ENDPOINT_INFO> cache_endpoint_info =
          custom_endpoint_cache.get(this->custom_endpoint_host_info->get_host(), nullptr);

        if (cache_endpoint_info != nullptr && cache_endpoint_info == endpoint_info) {
          const long long elapsed_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
          std::this_thread::sleep_for(
            std::chrono::nanoseconds(std::max(static_cast<long long>(0), this->refresh_rate_nanos - elapsed_time)));
          continue;
        }

        MYLOG_TRACE(this->logger, 0, "Detected change in custom endpoint info for '%s':\n{%s}",
                    custom_endpoint_host_info->get_host().c_str(), endpoint_info->to_string().c_str());

        // The custom endpoint info has changed, so we need to update the set of allowed/blocked hosts.
        std::shared_ptr<ALLOWED_AND_BLOCKED_HOSTS> allowed_and_blocked_hosts;
        if (endpoint_info->get_member_list_type() == STATIC_LIST) {
          allowed_and_blocked_hosts =
            std::make_shared<ALLOWED_AND_BLOCKED_HOSTS>(endpoint_info->get_static_members(), std::set<std::string>());
        } else {
          allowed_and_blocked_hosts =
            std::make_shared<ALLOWED_AND_BLOCKED_HOSTS>(std::set<std::string>(), endpoint_info->get_excluded_members());
        }

        custom_endpoint_cache.put(this->custom_endpoint_host_info->get_host(), endpoint_info,
                                  CUSTOM_ENDPOINT_INFO_EXPIRATION_NANOS);
        const long long elapsed_time =
          std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        std::this_thread::sleep_for(
          std::chrono::nanoseconds(std::max(static_cast<long long>(0), this->refresh_rate_nanos - elapsed_time)));
      }

    } catch (const std::exception& e) {
      // Log and continue monitoring.
      if (this->enable_logging) {
        MYLOG_TRACE(this->logger, 0, "Error while monitoring custom endpoint: %s", e.what());
      }
    }
  });
}

std::string CUSTOM_ENDPOINT_MONITOR::get_endpoints_as_string(
  const std::vector<Aws::RDS::Model::DBClusterEndpoint>& custom_endpoints) {
  std::string endpoints("[");

  for (auto const& e : custom_endpoints) {
    endpoints += e.GetDBClusterEndpointIdentifier();
    endpoints += ",";
  }
  if (endpoints.empty()) {
    endpoints = "<no endpoints>";
  } else {
    endpoints.pop_back();
    endpoints += "]";
  }
  return endpoints;
}

void CUSTOM_ENDPOINT_MONITOR::stop() {
  this->should_stop.store(true);
  this->thread_pool.stop(true);
  this->thread_pool.resize(0);
  custom_endpoint_cache.remove(this->custom_endpoint_host_info->get_host());
  --SDK_HELPER;
  MYLOG_TRACE(this->logger, 0, "Stopped custom endpoint monitor for '%s'", this->custom_endpoint_host_info->get_host());
}

void CUSTOM_ENDPOINT_MONITOR::clear_cache() { custom_endpoint_cache.clear(); }
