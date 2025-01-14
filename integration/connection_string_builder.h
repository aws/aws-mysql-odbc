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

#ifndef __CONNECTIONSTRINGBUILDER_H__
#define __CONNECTIONSTRINGBUILDER_H__

#include <stdexcept>
#include <string>

class ConnectionStringBuilder {
 public:
  ConnectionStringBuilder(const std::string& dsn, const std::string& server, int port) {
    length += sprintf(conn_in, "DSN=%s;SERVER=%s;PORT=%d;", dsn.c_str(), server.c_str(), port);
  }

  ConnectionStringBuilder(const std::string& str) { length += sprintf(conn_in, "%s", str.c_str()); }

  ConnectionStringBuilder& withUID(const std::string& uid) {
    length += sprintf(conn_in + length, "UID=%s;", uid.c_str());
    return *this;
  }

  ConnectionStringBuilder& withPWD(const std::string& pwd) {
    length += sprintf(conn_in + length, "PWD=%s;", pwd.c_str());
    return *this;
  }

  ConnectionStringBuilder& withDatabase(const std::string& db) {
    length += sprintf(conn_in + length, "DATABASE=%s;", db.c_str());
    return *this;
  }

  ConnectionStringBuilder& withLogQuery(const bool& log_query) {
    length += sprintf(conn_in + length, "LOG_QUERY=%d;", log_query ? 1 : 0);
    return *this;
  }

  ConnectionStringBuilder& withFailoverMode(const std::string& failover_mode) {
    length += sprintf(conn_in + length, "FAILOVER_MODE=%s;", failover_mode.c_str());
    return *this;
  }

  ConnectionStringBuilder& withMultiStatements(const bool& multi_statements) {
    length += sprintf(conn_in + length, "MULTI_STATEMENTS=%d;", multi_statements ? 1 : 0);
    return *this;
  }

  ConnectionStringBuilder& withEnableClusterFailover(const bool& enable_cluster_failover) {
    length += sprintf(conn_in + length, "ENABLE_CLUSTER_FAILOVER=%d;", enable_cluster_failover ? 1 : 0);
    return *this;
  }

  ConnectionStringBuilder& withFailoverTimeout(const int& failover_t) {
    length += sprintf(conn_in + length, "FAILOVER_TIMEOUT=%d;", failover_t);
    return *this;
  }

  ConnectionStringBuilder& withConnectTimeout(const int& connect_timeout) {
    length += sprintf(conn_in + length, "CONNECT_TIMEOUT=%d;", connect_timeout);
    return *this;
  }

  ConnectionStringBuilder& withNetworkTimeout(const int& network_timeout) {
    length += sprintf(conn_in + length, "NETWORK_TIMEOUT=%d;", network_timeout);
    return *this;
  }

  ConnectionStringBuilder& withHostPattern(const std::string& host_pattern) {
    length += sprintf(conn_in + length, "HOST_PATTERN=%s;", host_pattern.c_str());
    return *this;
  }

  ConnectionStringBuilder& withEnableFailureDetection(const bool& enable_failure_detection) {
    length += sprintf(conn_in + length, "ENABLE_FAILURE_DETECTION=%d;", enable_failure_detection ? 1 : 0);
    return *this;
  }

  ConnectionStringBuilder& withFailureDetectionTime(const int& failure_detection_time) {
    length += sprintf(conn_in + length, "FAILURE_DETECTION_TIME=%d;", failure_detection_time);
    return *this;
  }

  ConnectionStringBuilder& withFailureDetectionTimeout(const int& failure_detection_timeout) {
    length += sprintf(conn_in + length, "FAILURE_DETECTION_TIMEOUT=%d;", failure_detection_timeout);
    return *this;
  }

  ConnectionStringBuilder& withFailureDetectionInterval(const int& failure_detection_interval) {
    length += sprintf(conn_in + length, "FAILURE_DETECTION_INTERVAL=%d;", failure_detection_interval);
    return *this;
  }

  ConnectionStringBuilder& withFailureDetectionCount(const int& failure_detection_count) {
    length += sprintf(conn_in + length, "FAILURE_DETECTION_COUNT=%d;", failure_detection_count);
    return *this;
  }

  ConnectionStringBuilder& withMonitorDisposalTime(const int& monitor_disposal_time) {
    length += sprintf(conn_in + length, "MONITOR_DISPOSAL_TIME=%d;", monitor_disposal_time);
    return *this;
  }

  ConnectionStringBuilder& withReadTimeout(const int& read_timeout) {
    length += sprintf(conn_in + length, "READTIMEOUT=%d;", read_timeout);
    return *this;
  }

  ConnectionStringBuilder& withWriteTimeout(const int& write_timeout) {
    length += sprintf(conn_in + length, "WRITETIMEOUT=%d;", write_timeout);
    return *this;
  }

  ConnectionStringBuilder& withAuthMode(const std::string& auth_mode) {
    length += sprintf(conn_in + length, "AUTHENTICATION_MODE=%s;", auth_mode.c_str());
    return *this;
  }

  ConnectionStringBuilder& withAuthRegion(const std::string& auth_region) {
    length += sprintf(conn_in + length, "AWS_REGION=%s;", auth_region.c_str());
    return *this;
  }

  ConnectionStringBuilder& withAuthHost(const std::string& auth_host) {
    length += sprintf(conn_in + length, "IAM_HOST=%s;", auth_host.c_str());
    return *this;
  }

  ConnectionStringBuilder& withAuthPort(const int& auth_port) {
    length += sprintf(conn_in + length, "IAM_PORT=%d;", auth_port);
    return *this;
  }

  ConnectionStringBuilder& withAuthExpiration(const int& auth_expiration) {
    length += sprintf(conn_in + length, "IAM_EXPIRATION_TIME=%d;", auth_expiration);
    return *this;
  }

  ConnectionStringBuilder& withSecretId(const std::string& secret_id) {
    length += sprintf(conn_in + length, "SECRET_ID=%s;", secret_id.c_str());
    return *this;
  }

  ConnectionStringBuilder& withEnableCustomEndpointMonitoring(const bool& enable_custom_endpoint_monitoring) {
    length +=
        sprintf(conn_in + length, "ENABLE_CUSTOM_ENDPOINT_MONITORING=%d;", enable_custom_endpoint_monitoring ? 1 : 0);
    return *this;
  }
  ConnectionStringBuilder& withCustomEndpointRegion(const std::string& region) {
    length += sprintf(conn_in + length, "CUSTOM_ENDPOINT_REGION=%s;", region.c_str());
    return *this;
  }

  ConnectionStringBuilder& withShouldWaitForInfo(const bool& should_wait_for_info) {
    length += sprintf(conn_in + length, "WAIT_FOR_CUSTOM_ENDPOINT_INFO=%d;", should_wait_for_info ? 1 : 0);
    return *this;
  }

  ConnectionStringBuilder& withCustomEndpointInfoRefreshRateMs(const long& custom_endpoint_info_refresh_rate_ms) {
    length +=
        sprintf(conn_in + length, "CUSTOM_ENDPOINT_INFO_REFRESH_RATE_MS=%d;", custom_endpoint_info_refresh_rate_ms);
    return *this;
  }

  ConnectionStringBuilder& withWaitOnCachedInfoDurationMs(const long& wait_on_cached_info_duration_ms) {
    length +=
        sprintf(conn_in + length, "WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS=%ld;", wait_on_cached_info_duration_ms);
    return *this;
  }

  ConnectionStringBuilder& withIdleMonitorExpirationMs(const long& idle_monitor_expiration_ms) {
    length += sprintf(conn_in + length, "CUSTOM_ENDPOINT_MONITOR_EXPIRATION_MS=%ld;", idle_monitor_expiration_ms);
    return *this;
  }

  std::string getString() const { return conn_in; }

 private:
  char conn_in[4096] = "\0";
  int length = 0;
};

#endif /* __CONNECTIONSTRINGBUILDER_H__ */