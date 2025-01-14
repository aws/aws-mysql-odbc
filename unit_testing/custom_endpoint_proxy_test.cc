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

#include <aws/core/Aws.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {
const std::string WRITER_CLUSTER_URL{"writer.cluster-XYZ.us-east-1.rds.amazonaws.com"};
const std::string CUSTOM_ENDPOINT_URL{"custom.cluster-custom-XYZ.us-east-1.rds.amazonaws.com"};
}

static SQLHENV env;

class CustomEndpointProxyTest : public testing::Test {
 protected:
  DBC* dbc;
  DataSource* ds;
  MOCK_CONNECTION_PROXY* mock_connection_proxy;

  static void SetUpTestSuite() { SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env); }

  static void TearDownTestSuite() { SQLFreeHandle(SQL_HANDLE_ENV, env); }

  void SetUp() override {
    allocate_odbc_handles(env, dbc, ds);
    ds->opt_ENABLE_CUSTOM_ENDPOINT_MONITORING = true;
    ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO = true;
    ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS = 10000;
    ds->opt_CUSTOM_ENDPOINT_MONITOR_EXPIRATION_MS = 60000;

    mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
  }

  void TearDown() override {
    cleanup_odbc_handles(nullptr, dbc, ds);
    delete mock_connection_proxy;
  }
};

TEST_F(CustomEndpointProxyTest, TestConnect_MonitorNotCreatedIfNotCustomEndpointHost) {
}
