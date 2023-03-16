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

#include "installer.h"

using namespace Aws::SecretsManager;

SECRETS_MANAGER_PROXY::SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds) : SECRETS_MANAGER_PROXY(
    dbc, ds, nullptr) {}


SECRETS_MANAGER_PROXY::SECRETS_MANAGER_PROXY(DBC* dbc, DataSource* ds, MYSQL_PROXY* next_proxy) : MYSQL_PROXY(dbc, ds) {
    Aws::InitAPI(this->SDK_options);

    Aws::Client::ClientConfiguration config;
    const char* region = ds_get_utf8attr(ds->auth_region, &ds->auth_region8);
    config.region = region ? region : "us-east-1";

    this->sm_client = SecretsManagerClient(config);

    const Aws::String secret_ID = ds_get_utf8attr(ds->auth_secret_id, &ds->auth_secret_id8);

    Model::GetSecretValueRequest request;
    request.SetSecretId(secret_ID);

    auto getSecretValueOutcome = this->sm_client.GetSecretValue(request);
    if (getSecretValueOutcome.IsSuccess()) {
        std::cout << "Secret is: " << getSecretValueOutcome.GetResult().GetSecretString() << std::endl;
    }
    else {
        std::cout << "Failed with Error: " << getSecretValueOutcome.GetError() << std::endl;
    }

    this->next_proxy = next_proxy;
}

SECRETS_MANAGER_PROXY::~SECRETS_MANAGER_PROXY() {
    Aws::ShutdownAPI(this->SDK_options);
}

bool SECRETS_MANAGER_PROXY::real_connect(const char* host, const char* user, const char* passwd, const char* db, unsigned int port, const char* unix_socket, unsigned long clientflag) {

    const bool ret = next_proxy->real_connect(host, user, passwd, db, port, unix_socket, clientflag);
    return ret;
}

void SECRETS_MANAGER_PROXY::set_next_proxy(MYSQL_PROXY* next_proxy) {
    MYSQL_PROXY::set_next_proxy(next_proxy);
}
