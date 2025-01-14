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
// You should have received a scopy of the GNU General Public License
// along with this program. If not, see
// http://www.gnu.org/licenses/gpl-2.0.html.

#ifndef __CUSTOM_ENDPOINT_PROXY_H__
#define __CUSTOM_ENDPOINT_PROXY_H__

#include <aws/rds/RDSClient.h>
#include "connection_proxy.h"
#include "custom_endpoint_monitor.h"
#include "driver.h"
#include "sliding_expiration_cache_with_clean_up_thread.h"

class CUSTOM_ENDPOINT_PROXY : public CONNECTION_PROXY {
 public:
  CUSTOM_ENDPOINT_PROXY(DBC* dbc, DataSource* ds);
  CUSTOM_ENDPOINT_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy);

  bool real_connect(const char* host, const char* user, const char* password, const char* database, unsigned int port,
                    const char* socket, unsigned long flags) override;
  bool real_connect_dns_srv(
      const char* dns_srv_name, const char* user,
      const char* passwd, const char* db, unsigned long client_flag) override;
  int query(const char* q) override;
  int real_query(const char* q, unsigned long length) override;

  class CUSTOM_ENDPOINTS_SHOULD_DISPOSE_FUNC : public SHOULD_DISPOSE_FUNC<std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>> {
   public:
    bool should_dispose(std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> item) override { return true; }
  };

  class CUSTOM_ENDPOINTS_ITEM_DISPOSAL_FUNC : public ITEM_DISPOSAL_FUNC<std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>> {
   public:
    void dispose(const std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> monitor) override {
      try {
        monitor->stop();
      } catch (const std::exception& e) {
        // Ignore
      }
    }
  };
  static constexpr long long CACHE_CLEANUP_RATE_NANO = 60000000000;

 protected:
  std::string custom_endpoint_id;
  std::string region;
  std::string custom_endpoint_host;
  std::shared_ptr<Aws::RDS::RDSClient> rds_client;
  bool should_wait_for_info;
  long wait_on_cached_info_duration_ms;
  long idle_monitor_expiration_ms;

  static SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<std::string, std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>> monitors;

  std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> create_monitor_if_absent(DataSource* ds);

  /**
   * If custom endpoint info does not exist for the current custom endpoint, waits a short time for the info to be
   * made available by the custom endpoint monitor. This is necessary so that other plugins can rely on accurate custom
   * endpoint info. Since custom endpoint monitors and information are shared, we should not have to wait often.
   */
  void wait_for_custom_endpoint_info(std::shared_ptr<CUSTOM_ENDPOINT_MONITOR> monitor);

 private:
  std::shared_ptr<FILE> logger;
};

#endif
