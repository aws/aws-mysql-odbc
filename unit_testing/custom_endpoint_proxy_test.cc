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
}  // namespace

static SQLHENV env;
static Aws::SDKOptions options;

class CustomEndpointProxyTest : public testing::Test {
 protected:
  DBC* dbc;
  DataSource* ds;
  MOCK_CONNECTION_PROXY* mock_connection_proxy;
  ctpl::thread_pool monitor_thread_pool;
  std::shared_ptr<MOCK_CUSTOM_ENDPOINT_MONITOR> mock_monitor =
      std::make_shared<MOCK_CUSTOM_ENDPOINT_MONITOR>(monitor_thread_pool);

  static void SetUpTestSuite() {}

  static void TearDownTestSuite() { mysql_library_end(); }

  void SetUp() override {
    allocate_odbc_handles(env, dbc, ds);
    ds->opt_ENABLE_CUSTOM_ENDPOINT_MONITORING = true;
    ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO = true;
    ds->opt_CUSTOM_ENDPOINT_MONITOR_EXPIRATION_MS = 60000;

    mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
  }

  void TearDown() override {
    TEST_UTILS::get_custom_endpoint_monitor_cache().release_resources();
    cleanup_odbc_handles(env, dbc, ds);
  }
};

TEST_F(CustomEndpointProxyTest, TestConnect_MonitorNotCreatedIfNotCustomEndpointHost) {
  TEST_CUSTOM_ENDPOINT_PROXY custom_endpoint_proxy(dbc, ds, mock_connection_proxy);
  EXPECT_EQ(TEST_UTILS::get_custom_endpoint_monitor_cache().size(), 0);
  custom_endpoint_proxy.connect(WRITER_CLUSTER_URL.c_str(), "", "", "", 3306, "", 0);

  EXPECT_EQ(TEST_UTILS::get_custom_endpoint_monitor_cache().size(), 0);
  EXPECT_CALL(*mock_connection_proxy, connect(_, _, _, _, _, _, _)).Times(0);
}

TEST_F(CustomEndpointProxyTest, TestConnect_MonitorCreated) {
  TEST_CUSTOM_ENDPOINT_PROXY custom_endpoint_proxy(dbc, ds, mock_connection_proxy);
  EXPECT_EQ(0, TEST_UTILS::get_custom_endpoint_monitor_cache().size());
  EXPECT_CALL(custom_endpoint_proxy, create_custom_endpoint_monitor(_)).WillOnce(Return(mock_monitor));
  EXPECT_CALL(*mock_connection_proxy, connect(_, _, _, _, _, _, _)).Times(1);
  custom_endpoint_proxy.connect(CUSTOM_ENDPOINT_URL.c_str(), "", "", "", 3306, "", 0);

  EXPECT_EQ(TEST_UTILS::get_custom_endpoint_monitor_cache().size(), 1);
}

TEST_F(CustomEndpointProxyTest, TestConnect_TimeoutWaitingForInfo) {
  TEST_CUSTOM_ENDPOINT_PROXY custom_endpoint_proxy(dbc, ds, mock_connection_proxy);
  ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS = 100;
  EXPECT_EQ(TEST_UTILS::get_custom_endpoint_monitor_cache().size(), 0);
  EXPECT_CALL(custom_endpoint_proxy, create_custom_endpoint_monitor(_)).WillOnce(Return(mock_monitor));
  ds->opt_WAIT_FOR_CUSTOM_ENDPOINT_INFO_TIMEOUT_MS = 1;
  custom_endpoint_proxy.connect(CUSTOM_ENDPOINT_URL.c_str(), "user", "pwd", "db", 3306, "", 0);

  EXPECT_EQ(TEST_UTILS::get_custom_endpoint_monitor_cache().size(), 1);
  EXPECT_CALL(*mock_connection_proxy, connect(_, _, _, _, _, _, _)).Times(0);
}
