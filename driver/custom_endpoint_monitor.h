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

#ifndef __CUSTOM_ENDPOINT_MONITOR_H__
#define __CUSTOM_ENDPOINT_MONITOR_H__

#include <aws/rds/RDSClient.h>

#include <ctpl_stl.h>
#include "cache_map.h"
#include "connection_handler.h"
#include "connection_proxy.h"
#include "custom_endpoint_info.h"
#include "host_info.h"

class CUSTOM_ENDPOINT_MONITOR : public std::enable_shared_from_this<CUSTOM_ENDPOINT_MONITOR> {
 public:
  CUSTOM_ENDPOINT_MONITOR(const std::shared_ptr<HOST_INFO>& custom_endpoint_host_info, const std::string& endpoint_identifier,
                          const std::string& region, DataSource* ds, bool enable_logging = false);
  ~CUSTOM_ENDPOINT_MONITOR() = default;

  static bool should_dispose();
  bool has_custom_endpoint_info() const;
  void stop();
  void run();
  static void clear_cache();

 protected:
  static CACHE_MAP<std::string, std::shared_ptr<CUSTOM_ENDPOINT_INFO>> custom_endpoint_cache;
  static constexpr long long CUSTOM_ENDPOINT_INFO_EXPIRATION_NANOS = 300000000000;  // 5 minutes
  std::shared_ptr<HOST_INFO> custom_endpoint_host_info;
  std::string endpoint_identifier;
  std::string region;
  long long refresh_rate_nanos;
  bool enable_logging;
  std::shared_ptr<FILE> logger;
  ctpl::thread_pool thread_pool;
  std::atomic_bool should_stop{false};
  std::shared_ptr<Aws::RDS::RDSClient> rds_client;

 private:
  static std::string get_endpoints_as_string(const std::vector<Aws::RDS::Model::DBClusterEndpoint>& custom_endpoints);
};

#endif
