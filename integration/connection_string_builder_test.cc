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

#include <gtest/gtest.h>

#include "connection_string_builder.h"

class ConnectionStringBuilderTest : public testing::Test {};

// All connection string fields are set in the builder.
TEST_F(ConnectionStringBuilderTest, test_complete_string) {
  ConnectionStringBuilder builder("testDSN", "testServer", 3306);
  const std::string connection_string = builder.withUID("testUser")
                                            .withPWD("testPwd")
                                            .withDatabase("testDb")
                                            .withLogQuery(false)
                                            .withMultiStatements(false)
                                            .withEnableClusterFailover(true)
                                            .withFailoverTimeout(120000)
                                            .withConnectTimeout(20)
                                            .withNetworkTimeout(20)
                                            .withHostPattern("?.testDomain")
                                            .withEnableFailureDetection(true)
                                            .withFailureDetectionTime(10000)
                                            .withFailureDetectionInterval(100)
                                            .withFailureDetectionCount(4)
                                            .withMonitorDisposalTime(300)
                                            .getString();

  const std::string expected =
      "DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;DATABASE=testDb;LOG_QUERY=0;MULTI_STATEMENTS=0;"
      "ENABLE_CLUSTER_FAILOVER=1;FAILOVER_TIMEOUT=120000;CONNECT_TIMEOUT=20;NETWORK_TIMEOUT=20;HOST_PATTERN=?."
      "testDomain;ENABLE_FAILURE_DETECTION=1;FAILURE_DETECTION_TIME=10000;FAILURE_DETECTION_INTERVAL=100;FAILURE_"
      "DETECTION_COUNT=4;MONITOR_DISPOSAL_TIME=300;";
  EXPECT_EQ(0, expected.compare(connection_string));
}

// No optional fields are set in the builder. Build will succeed. Connection string with required fields.
TEST_F(ConnectionStringBuilderTest, test_only_required_fields) {
  ConnectionStringBuilder builder("testDSN", "testServer", 3306);
  const std::string connection_string = builder.getString();

  const std::string expected = "DSN=testDSN;SERVER=testServer;PORT=3306;";
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Some optional fields are set and others not set in the builder. Build will succeed.
// Connection string with required fields and ONLY the fields that were set.
TEST_F(ConnectionStringBuilderTest, test_some_optional) {
  ConnectionStringBuilder builder("testDSN", "testServer", 3306);
  const std::string connection_string = builder.withUID("testUser").withPWD("testPwd").getString();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Boolean values are set in the builder. Build will succeed. True will be marked as 1 in the string, false 0.
TEST_F(ConnectionStringBuilderTest, test_setting_boolean_fields) {
  ConnectionStringBuilder builder("testDSN", "testServer", 3306);
  const std::string connection_string = builder.withUID("testUser")
                                            .withPWD("testPwd")
                                            .withLogQuery(false)
                                            .withMultiStatements(true)
                                            .withEnableClusterFailover(false)
                                            .withEnableFailureDetection(true)
                                            .getString();

  const std::string expected(
      "DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=0;MULTI_STATEMENTS=1;ENABLE_CLUSTER_"
      "FAILOVER=0;ENABLE_FAILURE_DETECTION=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder with required values. Then append other properties to the builder. Then build the connection string.
// Build will succeed.
TEST_F(ConnectionStringBuilderTest, test_setting_multiple_steps_1) {
  ConnectionStringBuilder builder("testDSN", "testServer", 3306);
  const std::string connection_string = builder.withUID("testUser").withPWD("testPwd").withLogQuery(true).getString();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}
