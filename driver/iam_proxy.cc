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

#include <functional>

#include "driver.h"
#include "iam_proxy.h"

std::unordered_map<std::string, TOKEN_INFO> IAM_PROXY::token_cache;
std::mutex IAM_PROXY::token_cache_mutex;

IAM_PROXY::IAM_PROXY(DBC* dbc, DataSource* ds) : IAM_PROXY(dbc, ds, nullptr) {};

IAM_PROXY::IAM_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  if (ds->opt_AUTH_REGION) {
    this->auth_util = std::make_shared<AUTH_UTIL>((const char*)ds->opt_AUTH_REGION);
  } else {
    this->auth_util = std::make_shared<AUTH_UTIL>();
  }
}

IAM_PROXY::~IAM_PROXY() {
    this->auth_util.reset();
}

#ifdef UNIT_TEST_BUILD
IAM_PROXY::IAM_PROXY(DBC *dbc, DataSource *ds, CONNECTION_PROXY *next_proxy,
                     std::shared_ptr<AUTH_UTIL> auth_util) : CONNECTION_PROXY(dbc, ds) {

    this->next_proxy = next_proxy;
    this->auth_util = auth_util;
}
#endif

bool IAM_PROXY::connect(const char* host, const char* user, const char* password,
                        const char* database, unsigned int port, const char* socket, unsigned long flags) {

    auto f = std::bind(&CONNECTION_PROXY::connect, next_proxy, host, user, std::placeholders::_1, database, port,
                       socket, flags);
    return invoke_func_with_generated_token(f);
}

bool IAM_PROXY::change_user(const char* user, const char* passwd, const char* db) {
    auto f = std::bind(&CONNECTION_PROXY::change_user, next_proxy, user, std::placeholders::_1, db);
    return invoke_func_with_generated_token(f);
}

std::string IAM_PROXY::get_auth_token(
    const char* host, const char* region, unsigned int port,
    const char* user, unsigned int time_until_expiration,
    bool force_generate_new_token) {

    if (!host) {
        host = "";
    }
    if (!region) {
        region = "";
    }
    if (!user) {
        user = "";
    }

    std::string auth_token;
    std::string cache_key = this->auth_util->build_cache_key(host, region, port, user);
    using_cached_token = false;

    {
        std::unique_lock<std::mutex> lock(token_cache_mutex);

        if (force_generate_new_token) {
            token_cache.erase(cache_key);
        }
        else {
            // Search for token in cache
            auto find_token = token_cache.find(cache_key);
            if (find_token != token_cache.end()) {
                TOKEN_INFO info = find_token->second;
                if (info.is_expired()) {
                    token_cache.erase(cache_key);
                } else {
                    using_cached_token = true;
                    return info.token;
                }
            }
        }

        // Generate new token
        auth_token = this->auth_util->get_auth_token(host, region, port, user);

        token_cache[cache_key] = TOKEN_INFO(auth_token, time_until_expiration);
    }

    return auth_token;
}

void IAM_PROXY::clear_token_cache() {
    std::unique_lock<std::mutex> lock(token_cache_mutex);
    token_cache.clear();
}

bool IAM_PROXY::invoke_func_with_generated_token(std::function<bool(const char*)> func) {

    // Use user provided auth host if present, otherwise, use server host
  const char *AUTH_HOST = ds->opt_AUTH_HOST ? (const char *)ds->opt_AUTH_HOST
                                            : (const char *)ds->opt_SERVER;

  // Go with default region if region is not provided.
  const char *region = ds->opt_AUTH_REGION ? (const char *)ds->opt_AUTH_REGION
                                           : Aws::Region::US_EAST_1;

    int iam_port = ds->opt_AUTH_PORT;
    if (iam_port == UNDEFINED_PORT) {
        // Use regular port if user does not provide IAM port
        iam_port = ds->opt_PORT;
    }

    std::string auth_token = this->get_auth_token(AUTH_HOST, region, iam_port, 
                                                  (const char*)ds->opt_UID, ds->opt_AUTH_EXPIRATION);

    bool connect_result = func(auth_token.c_str());
    if (!connect_result) {
        if (using_cached_token) {
            // Retry func with a fresh token
            auth_token = this->get_auth_token(AUTH_HOST, region, iam_port, (const char*)ds->opt_UID,
                                              ds->opt_AUTH_EXPIRATION, true);
            if (func(auth_token.c_str())) {
                return true;
            }
        }
        
        Aws::Auth::DefaultAWSCredentialsProviderChain credentials_provider;
        Aws::Auth::AWSCredentials credentials = credentials_provider.GetAWSCredentials();
        if (credentials.IsEmpty()) {
            this->set_custom_error_message(
                "Could not find AWS Credentials for IAM Authentication. Please set up AWS credentials.");
        }
        else if (credentials.IsExpired()) {
            this->set_custom_error_message(
                "AWS Credentials for IAM Authentication are expired. Please refresh AWS credentials.");
        }
    }

    return connect_result;
}
