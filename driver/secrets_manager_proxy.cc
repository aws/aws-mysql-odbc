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

#include <aws/secretsmanager/SecretsManagerServiceClientModel.h>
#include <aws/secretsmanager/model/GetSecretValueRequest.h>

#include "secrets_manager_proxy.h"

#include "mylog.h"

#undef GetMessage // workaround for AWSError method GetMessage()

using namespace Aws::SecretsManager;

namespace {
    const char* DEFAULT_REGION = "us-east-1";
    const Aws::String USERNAME_KEY{ "username" };
    const Aws::String PASSWORD_KEY{"password"};
}

SECRETS_MANAGER_PROXY::SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds) : SECRETS_MANAGER_PROXY(dbc, ds, nullptr) {}


SECRETS_MANAGER_PROXY::SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CONNECTION_PROXY(
    dbc, ds) {

    SecretsManagerClientConfiguration config;
    const char* region = ds_get_utf8attr(ds->auth_region, &ds->auth_region8);
    config.region = region ? region : DEFAULT_REGION;

    this->sm_client = SecretsManagerClient(config);
    this->next_proxy = next_proxy;
}

bool SECRETS_MANAGER_PROXY::real_connect(const char* host, const char* user, const char* passwd, const char* db,
                                         unsigned int port, const char* unix_socket, unsigned long clientflag) {

    const auto json_value = parse_json_value(fetch_latest_credentials());
    const auto json_view = json_value.View();
    const auto username = get_from_json_view(json_view, USERNAME_KEY);
    const auto password = get_from_json_view(json_view, PASSWORD_KEY);

    return next_proxy->real_connect(host, username.c_str(), password.c_str(), db, port, unix_socket, clientflag);
}

bool SECRETS_MANAGER_PROXY::real_connect_dns_srv(const char* dns_srv_name, const char* user, const char* passwd,
                                                 const char* db, unsigned long client_flag) {

    const auto json_value = parse_json_value(fetch_latest_credentials());
    const auto json_view = json_value.View();
    const auto username = get_from_json_view(json_view, USERNAME_KEY);
    const auto password = get_from_json_view(json_view, PASSWORD_KEY);

    return next_proxy->real_connect_dns_srv(dns_srv_name, username.c_str(), password.c_str(), db, client_flag);
}

Aws::String SECRETS_MANAGER_PROXY::fetch_latest_credentials() const {
    const Aws::String secret_ID = ds_get_utf8attr(ds->auth_secret_id, &ds->auth_secret_id8);

    Model::GetSecretValueRequest request;
    request.SetSecretId(secret_ID);

    Aws::String secret_string;

    auto get_secret_value_outcome = this->sm_client.GetSecretValue(request);

    if (get_secret_value_outcome.IsSuccess()) {
        secret_string = get_secret_value_outcome.GetResult().GetSecretString();
    }
    else {
        MYLOG_DBC_TRACE(dbc, get_secret_value_outcome.GetError().GetMessage().c_str());
    }
    return secret_string;
}

Aws::Utils::Json::JsonValue SECRETS_MANAGER_PROXY::parse_json_value(Aws::String json_string) const {
    auto res_json = Aws::Utils::Json::JsonValue(json_string);
    if (!res_json.WasParseSuccessful()) {
        MYLOG_DBC_TRACE(dbc, res_json.GetErrorMessage().c_str());
        throw std::runtime_error("Error parsing response body. " + res_json.GetErrorMessage());
    }
    return res_json;
}

std::string SECRETS_MANAGER_PROXY::get_from_json_view(Aws::Utils::Json::JsonView view, std::string key) const {
    std::string value;
    if (view.ValueExists(key)) {
        value = view.GetString(key);
    }
    else {
        const auto error = "Unable to extract the " + key + " from secrets manager response.";
        MYLOG_DBC_TRACE(dbc, error.c_str());
        throw std::runtime_error(error);
    }
    return value;
}
