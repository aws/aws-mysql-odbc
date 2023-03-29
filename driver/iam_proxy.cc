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

#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include "aws_sdk_helper.h"
#include "driver.h"
#include "iam_proxy.h"

namespace {
    AWS_SDK_HELPER SDK_HELPER;
}

std::unordered_map<std::string, TOKEN_INFO> IAM_PROXY::token_cache;
std::mutex IAM_PROXY::token_cache_mutex;

IAM_PROXY::IAM_PROXY(DBC* dbc, DataSource* ds) : IAM_PROXY(dbc, ds, nullptr) {};

IAM_PROXY::IAM_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CONNECTION_PROXY(dbc, ds) {
    ++SDK_HELPER;

    this->next_proxy = next_proxy;

    Aws::Auth::DefaultAWSCredentialsProviderChain credentials_provider;
    Aws::Auth::AWSCredentials credentials = credentials_provider.GetAWSCredentials();

    Aws::Client::ClientConfiguration client_config;
    if (ds->auth_region) {
        client_config.region = ds_get_utf8attr(ds->auth_region, &ds->auth_region8);
    }

    this->rds_client = std::make_shared<Aws::RDS::RDSClient>(credentials, client_config);
}

IAM_PROXY::~IAM_PROXY() {
    this->rds_client.reset();
    --SDK_HELPER;
}

bool IAM_PROXY::connect(const char* host, const char* user, const char* password,
    const char* database, unsigned int port, const char* socket, unsigned long flags) {

    if (ds->auth_host) {
        host = ds_get_utf8attr(ds->auth_host, &ds->auth_host8);
    }

    const char* region;
    if (ds->auth_region8) {
        region = (const char*)ds->auth_region8;
    }
    else {
        // Go with default region if region is not provided.
        region = "us-east-1";
    }

    port = ds->auth_port;

    std::string auth_token = this->get_auth_token(host, region, port, user, ds->auth_expiration);

    bool connect_result = next_proxy->connect(host, user, auth_token.c_str(), database, port, socket, flags);
    if (!connect_result) {
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

std::string IAM_PROXY::get_auth_token(
    const char* host, const char* region, unsigned int port,
    const char* user, unsigned int time_until_expiration) {

    std::string auth_token;
    std::string cache_key = build_cache_key(host, region, port, user);

    {
        std::unique_lock<std::mutex> lock(token_cache_mutex);

        // Search for token in cache
        auto find_token = token_cache.find(cache_key);
        if (find_token != token_cache.end())
        {
            TOKEN_INFO info = find_token->second;
            if (info.is_expired()) {
                token_cache.erase(cache_key);
            }
            else {
                return info.token;
            }
        }

        // Generate new token
        auth_token = generate_auth_token(host, region, port, user);

        token_cache[cache_key] = TOKEN_INFO(auth_token, time_until_expiration);
    }

    return auth_token;
}

std::string IAM_PROXY::build_cache_key(
    const char* host, const char* region, unsigned int port, const char* user) {

    // Format should be "<region>:<host>:<port>:<user>"
    return std::string(region)
        .append(":").append(host)
        .append(":").append(std::to_string(port))
        .append(":").append(user);
}

std::string IAM_PROXY::generate_auth_token(
    const char* host, const char* region, unsigned int port, const char* user) {

    return this->rds_client->GenerateConnectAuthToken(host, region, port, user);
}

void IAM_PROXY::clear_token_cache() {
    std::unique_lock<std::mutex> lock(token_cache_mutex);
    token_cache.clear();
}
