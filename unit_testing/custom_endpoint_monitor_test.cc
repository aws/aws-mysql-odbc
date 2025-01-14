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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

#include <aws/rds/model/DescribeDBClusterEndpointsRequest.h>
#include <aws/rds/model/DescribeDBClusterEndpointsResult.h>

#include "driver/custom_endpoint_monitor.h"
#include "test_utils.h"
#include "mock_objects.h"

using namespace Aws::RDS;

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {
const std::string WRITER_CLUSTER_URL{"writer.cluster-XYZ.us-east-1.rds.amazonaws.com"};
const std::string CUSTOM_ENDPOINT_URL{"custom.cluster-custom-XYZ.us-east-1.rds.amazonaws.com"};
const auto ENDPOINT = Aws::Utils::Json::JsonValue(CUSTOM_ENDPOINT_URL);

const long long REFRESH_RATE_NANOS = 50000000;
}  // namespace

static SQLHENV env;
static Aws::SDKOptions sdk_options;

class CustomEndpointMonitorTest : public testing::Test {
 protected:
  DBC* dbc;
  DataSource* ds;
  MOCK_CONNECTION_PROXY* mock_connection_proxy;
  std::shared_ptr<MOCK_RDS_CLIENT> mock_rds_client;

  static void SetUpTestSuite() {
    Aws::InitAPI(sdk_options);
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
  }

  static void TearDownTestSuite() {
    Aws::ShutdownAPI(sdk_options);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
  }

  void SetUp() override {
    allocate_odbc_handles(env, dbc, ds);
    mock_rds_client = std::make_shared<MOCK_RDS_CLIENT>();
    mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
  }

  void TearDown() override {
    cleanup_odbc_handles(nullptr, dbc, ds);
    TEST_UTILS::get_custom_endpoint_cache().clear();
    delete mock_connection_proxy;
  }
};

TEST_F(CustomEndpointMonitorTest, TestRun) {
  Model::DBClusterEndpoint endpoint;
  endpoint.AddStaticMembers(CUSTOM_ENDPOINT_URL);
  std::vector<Model::DBClusterEndpoint> endpoints{endpoint};

  const auto expected_result = Model::DescribeDBClusterEndpointsResult().WithDBClusterEndpoints(endpoints);
  const auto expected_outcome = Model::DescribeDBClusterEndpointsOutcome(expected_result);

  EXPECT_CALL(*mock_rds_client, DescribeDBClusterEndpoints(Property(
                                    "GetDBClusterEndpointIdentifier",
                                    &Model::DescribeDBClusterEndpointsRequest::GetDBClusterEndpointIdentifier,
                                    StrEq("custom"))))
      .WillRepeatedly(Return(expected_outcome));

  CUSTOM_ENDPOINT_MONITOR monitor(CUSTOM_ENDPOINT_URL, "custom", "us-east-1", REFRESH_RATE_NANOS, true,
                                  mock_rds_client);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  monitor.stop();
}
