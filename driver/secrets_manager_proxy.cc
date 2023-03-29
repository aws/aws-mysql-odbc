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

#include "aws_sdk_helper.h"
#include "secrets_manager_proxy.h"

#include "mylog.h"

#undef GetMessage // workaround for AWSError method GetMessage()

using namespace Aws::SecretsManager;

namespace {
    AWS_SDK_HELPER SDK_HELPER;
    const Aws::String USERNAME_KEY{ "username" };
    const Aws::String PASSWORD_KEY{ "password" };
}

std::map<std::pair<Aws::String, Aws::String>, Aws::Utils::Json::JsonValue> SECRETS_MANAGER_PROXY::secrets_cache;
std::mutex SECRETS_MANAGER_PROXY::secrets_cache_mutex;

SECRETS_MANAGER_PROXY::SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds) : CONNECTION_PROXY(dbc, ds) {
    ++SDK_HELPER;

    SecretsManagerClientConfiguration config;
    const auto region = ds_get_utf8attr(ds->auth_region, &ds->auth_region8);
    config.region = region ? region : Aws::Region::US_EAST_1;
    this->sm_client = std::make_shared<SecretsManagerClient>(config);

    const auto secret_ID = ds_get_utf8attr(ds->auth_secret_id, &ds->auth_secret_id8);
    if (!secret_ID) {
        const auto error = "Missing required config parameter for Secrets Manager: Secret ID";
        MYLOG_DBC_TRACE(dbc, error);
        this->set_custom_error_message(error);

    }
    this->secret_key = std::make_pair(secret_ID, config.region);
    this->next_proxy = nullptr;
}

#ifdef UNIT_TEST_BUILD
SECRETS_MANAGER_PROXY::SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy,
                                             std::shared_ptr<SecretsManagerClient> sm_client) :
    CONNECTION_PROXY(dbc, ds), sm_client{std::move(sm_client)} {

    const Aws::String region = ds_get_utf8attr(ds->auth_region, &ds->auth_region8);
    const Aws::String secret_ID = ds_get_utf8attr(ds->auth_secret_id, &ds->auth_secret_id8);
    this->secret_key = std::make_pair(secret_ID, region);
    this->next_proxy = next_proxy;
}
#endif

SECRETS_MANAGER_PROXY::~SECRETS_MANAGER_PROXY() {
    this->sm_client.reset();
    --SDK_HELPER;
}

bool SECRETS_MANAGER_PROXY::connect(const char* host, const char* user, const char* passwd, const char* database,
                                         unsigned int port, const char* unix_socket, unsigned long flags) {

    bool fetched = update_secret(false);
    std::string username = get_from_secret_json_value(USERNAME_KEY);
    std::string password = get_from_secret_json_value(PASSWORD_KEY);
    fetched = false;
    bool ret = next_proxy->connect(host, username.c_str(), password.c_str(), database, port, unix_socket, flags);

    if (!ret && next_proxy->error_code() == ER_ACCESS_DENIED_ERROR && !fetched) {
        // Login unsuccessful with cached credentials
        // Try to re-fetch credentials and try again
        fetched = update_secret(true);
        if (fetched) {
            username = get_from_secret_json_value(USERNAME_KEY);
            password = get_from_secret_json_value(PASSWORD_KEY);
            ret = next_proxy->connect(host, username.c_str(), password.c_str(), database, port, unix_socket, flags);
        }
    }

    return ret;
}

bool SECRETS_MANAGER_PROXY::update_secret(bool force_re_fetch) {
    bool fetched = false;
    {
        std::unique_lock<std::mutex> lock(secrets_cache_mutex);

        const auto search = secrets_cache.find(this->secret_key);
        if (search != secrets_cache.end() && !force_re_fetch) {
            this->secret_json_value = search->second;
        }
        else {
            this->secret_json_value = fetch_latest_credentials();
            fetched = true;
            secrets_cache[this->secret_key] = this->secret_json_value;
        }
    }

    return fetched;
}

Aws::Utils::Json::JsonValue SECRETS_MANAGER_PROXY::fetch_latest_credentials() const {
    Aws::String secret_string;

    Model::GetSecretValueRequest request;
    request.SetSecretId(this->secret_key.first);
    auto get_secret_value_outcome = this->sm_client->GetSecretValue(request);

    if (get_secret_value_outcome.IsSuccess()) {
        secret_string = get_secret_value_outcome.GetResult().GetSecretString();
    }
    else {
        MYLOG_DBC_TRACE(dbc, get_secret_value_outcome.GetError().GetMessage().c_str());
    }
    return parse_json_value(secret_string);
}

Aws::Utils::Json::JsonValue SECRETS_MANAGER_PROXY::parse_json_value(Aws::String json_string) const {
    auto res_json = Aws::Utils::Json::JsonValue(json_string);
    if (!res_json.WasParseSuccessful()) {
        MYLOG_DBC_TRACE(dbc, res_json.GetErrorMessage().c_str());
        throw std::runtime_error("Error parsing secrets manager response body. " + res_json.GetErrorMessage());
    }
    return res_json;
}

std::string SECRETS_MANAGER_PROXY::get_from_secret_json_value(std::string key) const {
    std::string value;
    const auto view = this->secret_json_value.View();

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
