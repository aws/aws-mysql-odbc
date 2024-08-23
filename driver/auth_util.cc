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

#include "auth_util.h"
#include "aws_sdk_helper.h"
#include "driver.h"

namespace {
AWS_SDK_HELPER SDK_HELPER;
}

AUTH_UTIL::AUTH_UTIL(const char* region) {
  ++SDK_HELPER;

  Aws::Auth::DefaultAWSCredentialsProviderChain credentials_provider;
  Aws::Auth::AWSCredentials credentials = credentials_provider.GetAWSCredentials();

  Aws::RDS::RDSClientConfiguration client_config;
  if (region) {
    client_config.region = region;
  }

  this->rds_client = std::make_shared<Aws::RDS::RDSClient>(credentials, client_config);
};

std::string AUTH_UTIL::get_auth_token(const char* host, const char* region, unsigned int port, const char* user) {
  return this->rds_client->GenerateConnectAuthToken(host, region, port, user);
}

std::string AUTH_UTIL::build_cache_key(const char* host, const char* region, unsigned int port, const char* user) {
  return std::string(region).append(":").append(host).append(":").append(std::to_string(port)).append(":").append(user);
}

AUTH_UTIL::~AUTH_UTIL() {
  this->rds_client.reset();
  --SDK_HELPER;
}
