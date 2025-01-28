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

#include "custom_endpoint_proxy.h"
#include "mylog.h"
#include "rds_utils.h"

SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<std::string, std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>>
    CUSTOM_ENDPOINT_PROXY::monitors(std::make_shared<CUSTOM_ENDPOINTS_SHOULD_DISPOSE_FUNC>(),
                                    std::make_shared<CUSTOM_ENDPOINTS_ITEM_DISPOSAL_FUNC>(), CACHE_CLEANUP_RATE_NANO);

bool CUSTOM_ENDPOINT_PROXY::is_monitor_cache_initialized(false);

CUSTOM_ENDPOINT_PROXY::CUSTOM_ENDPOINT_PROXY(DBC* dbc, DataSource* ds) : CUSTOM_ENDPOINT_PROXY(dbc, ds, nullptr) {}
CUSTOM_ENDPOINT_PROXY::CUSTOM_ENDPOINT_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy)
    : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  this->topology_service = dbc->get_topology_service();

  if (ds->opt_LOG_QUERY) {
    this->logger = init_log_file();
  }
  this->should_wait_for_info = ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO;
  this->wait_on_cached_info_duration_ms = ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS;
  this->idle_monitor_expiration_ms = ds->opt_CUSTOM_ENDPOINT_MONITOR_EXPIRATION_MS;

  if (!is_monitor_cache_initialized) {
    monitors.init_clean_up_thread();
    is_monitor_cache_initialized = true;
  }
}

bool CUSTOM_ENDPOINT_PROXY::connect(const char* host, const char* user, const char* password, const char* database,
                                    unsigned int port, const char* socket, unsigned long flags) {
  if (!RDS_UTILS::is_rds_custom_cluster_dns(host)) {
    return this->next_proxy->connect(host, user, password, database, port, socket, flags);
  }

  this->custom_endpoint_host = host;
  MYLOG_TRACE(this->logger, 0, "Detected a connection request to a custom endpoint URL: '%s'", host);

  this->custom_endpoint_id = RDS_UTILS::get_rds_cluster_id(host);

  if (this->custom_endpoint_id.empty()) {
    this->set_custom_error_message("Unable to parse custom endpoint identifier from URL.");
    return false;
  }

  this->region = ds->opt_CUSTOM_ENDPOINT_REGION ? static_cast<const char*>(ds->opt_CUSTOM_ENDPOINT_REGION)
                                                : RDS_UTILS::get_rds_region(host);
  if (this->region.empty()) {
    this->set_custom_error_message(
        "Unable to determine connection region. If you are using a non-standard RDS URL, please set the "
        "'custom_endpoint_region' property");
    return false;
  }

  const std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> monitor = create_monitor_if_absent(ds);
  if (this->should_wait_for_info) {
    // If needed, wait a short time for custom endpoint info to be discovered.
    this->wait_for_custom_endpoint_info(monitor);
  }
  return this->next_proxy->connect(host, user, password, database, port, socket, flags);
}

int CUSTOM_ENDPOINT_PROXY::query(const char* q) {
  if (!this->custom_endpoint_host.empty()) {
    const std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> monitor = create_monitor_if_absent(ds);
    if (this->should_wait_for_info) {
      // If needed, wait a short time for custom endpoint info to be discovered.
      this->wait_for_custom_endpoint_info(monitor);
    }
  }

  return next_proxy->query(q);
}

int CUSTOM_ENDPOINT_PROXY::real_query(const char* q, unsigned long length) {
  if (!this->custom_endpoint_host.empty()) {
    const std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> monitor = create_monitor_if_absent(ds);
    if (this->should_wait_for_info) {
      // If needed, wait a short time for custom endpoint info to be discovered.
      this->wait_for_custom_endpoint_info(monitor);
    }
  }

  return next_proxy->real_query(q, length);
}

void CUSTOM_ENDPOINT_PROXY::wait_for_custom_endpoint_info(std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> monitor) {
  bool has_custom_endpoint_info = monitor->has_custom_endpoint_info();

  if (has_custom_endpoint_info) {
    return;
  }

  // Wait for the monitor to place the custom endpoint info in the cache. This ensures other plugins get accurate
  // custom endpoint info.
  MYLOG_TRACE(this->logger, 0,
              "Custom endpoint info for '%s' was not found. Waiting %dms for the endpoint monitor to fetch info...",
              this->custom_endpoint_host.c_str(), this->wait_on_cached_info_duration_ms)

  const auto wait_for_endpoint_info_timeout_nanos =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(this->wait_on_cached_info_duration_ms);

  while (!has_custom_endpoint_info && std::chrono::steady_clock::now() < wait_for_endpoint_info_timeout_nanos) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    has_custom_endpoint_info = monitor->has_custom_endpoint_info();
  }

  if (!has_custom_endpoint_info) {
    char buf[1024];
    myodbc_snprintf(
        buf, sizeof(buf),
        "The custom endpoint plugin timed out after %ld ms while waiting for custom endpoint info for host %s.",
        this->wait_on_cached_info_duration_ms, this->custom_endpoint_host.c_str());

    set_custom_error_message(buf);
  }
}

std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> CUSTOM_ENDPOINT_PROXY::create_custom_endpoint_monitor(
    const long long refresh_rate_nanos) {
  return std::make_shared<CUSTOM_ENDPOINT_MONITOR>(this->topology_service, this->custom_endpoint_host,
                                                   this->custom_endpoint_id, this->region, refresh_rate_nanos,
                                                   this->dbc->env->custom_endpoint_thread_pool);
}

std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> CUSTOM_ENDPOINT_PROXY::create_monitor_if_absent(DataSource* ds) {
  const long long refresh_rate_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                           std::chrono::milliseconds(ds->opt_CUSTOM_ENDPOINT_INFO_REFRESH_RATE_MS))
                                           .count();

  return monitors.compute_if_absent(
      this->custom_endpoint_host,
      [=](std::string key) { return this->create_custom_endpoint_monitor(refresh_rate_nanos); },
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(this->idle_monitor_expiration_ms))
          .count());
}

void CUSTOM_ENDPOINT_PROXY::release_resources() {
  if (!monitors.empty()) {
    monitors.release_resources();
  }
}
