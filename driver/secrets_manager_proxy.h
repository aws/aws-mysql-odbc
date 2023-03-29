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

#ifndef __SECRETS_MANAGER_PROXY__
#define __SECRETS_MANAGER_PROXY__

#include <aws/core/Aws.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/secretsmanager/SecretsManagerClient.h>

#include <map>

#include "connection_proxy.h"
#include "driver.h"

static std::map<std::pair<Aws::String, Aws::String>, Aws::Utils::Json::JsonValue> secrets_cache;
static std::mutex secrets_cache_mutex;

class SECRETS_MANAGER_PROXY : public CONNECTION_PROXY {
public:
    SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds);
    SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy);

    bool real_connect(const char* host, const char* user,
                      const char* passwd, const char* db, unsigned int port,
                      const char* unix_socket, unsigned long clientflag) override;
    bool real_connect_dns_srv(const char* dns_srv_name,
                              const char* user, const char* passwd,
                              const char* db, unsigned long client_flag) override;

private:
    Aws::SecretsManager::SecretsManagerClient sm_client;
    std::pair<Aws::String, Aws::String> secret_key;
    Aws::Utils::Json::JsonValue secret_json_value;

    bool update_secret(bool force_re_fetch);
    Aws::Utils::Json::JsonValue fetch_latest_credentials() const;
    Aws::Utils::Json::JsonValue parse_json_value(Aws::String json_string) const;
    std::string get_from_secret_json_value(std::string key) const;
};

#endif /* __SECRETS_MANAGER_PROXY__ */
