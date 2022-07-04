/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <gtest/gtest.h>

#include "connection_string_builder.cc"

class ConnectionStringBuilderTest : public testing::Test {
};

// No fields are set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_empty_builder) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.build(), std::runtime_error);
}

// More than one required field is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_fields) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withServer("testServer").build(), std::runtime_error);
}

// Required field DSN is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_dsn) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withServer("testServer").withPort(3306).build(), std::runtime_error);
}

// Required field Server is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_server) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withDSN("testDSN").withPort(3306).build(), std::runtime_error);
}

// Required field Port is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_port) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withDSN("testDSN").withServer("testServer").build(), std::runtime_error);
}

// All connection string fields are set in the builder.
TEST_F(ConnectionStringBuilderTest, test_complete_string) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withServer("testServer")
                                   .withUID("testUser")
                                   .withPWD("testPwd")
                                   .withLogQuery(false)
                                   .withAllowReaderConnections(true)
                                   .withMultiStatements(false)
                                   .withDSN("testDSN")
                                   .withFailoverT(120000)
                                   .withPort(3306)
                                   .withDatabase("testDb")
                                   .withConnectTimeout(20)
                                   .withNetworkTimeout(20)
                                   .withHostPattern("?.testDomain")
                                   .withEnableFailureDetection(true)
                                   .withFailureDetectionTime(10000)
                                   .withFailureDetectionInterval(100)
                                   .withFailureDetectionCount(4)
                                   .withMonitorDisposalTime(300)
                                   .build();

  const std::string expected = "DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;DATABASE=testDb;LOG_QUERY=0;ALLOW_READER_CONNECTIONS=1;MULTI_STATEMENTS=0;FAILOVER_T=120000;CONNECT_TIMEOUT=20;NETWORK_TIMEOUT=20;HOST_PATTERN=?.testDomain;ENABLE_FAILURE_DETECTION=1;FAILURE_DETECTION_TIME=10000;FAILURE_DETECTION_INTERVAL=100;FAILURE_DETECTION_COUNT=4;MONITOR_DISPOSAL_TIME=300;";
  EXPECT_EQ(0, expected.compare(connection_string));
}

// No optional fields are set in the builder. Build will succeed. Connection string with required fields.
TEST_F(ConnectionStringBuilderTest, test_only_required_fields) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withDSN("testDSN")
                                   .withServer("testServer")
                                   .withPort(3306)
                                   .build();

  const std::string expected = "DSN=testDSN;SERVER=testServer;PORT=3306;";
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Some optional fields are set and others not set in the builder. Build will succeed.
// Connection string with required fields and ONLY the fields that were set.
TEST_F(ConnectionStringBuilderTest, test_some_optional) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withDSN("testDSN")
                                   .withServer("testServer")
                                   .withPort(3306)
                                   .withUID("testUser")
                                   .withPWD("testPwd")
                                   .build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Boolean values are set in the builder. Build will succeed. True will be marked as 1 in the string, false 0.
TEST_F(ConnectionStringBuilderTest, test_setting_boolean_fields) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withDSN("testDSN")
                                   .withServer("testServer")
                                   .withPort(3306)
                                   .withUID("testUser")
                                   .withPWD("testPwd")
                                   .withLogQuery(false)
                                   .withAllowReaderConnections(true)
                                   .withMultiStatements(true)
                                   .withEnableFailureDetection(true)
                                   .build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=0;ALLOW_READER_CONNECTIONS=1;MULTI_STATEMENTS=1;ENABLE_FAILURE_DETECTION=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder with required values. Then append other properties to the builder. Then build the connection string. Build will succeed.
TEST_F(ConnectionStringBuilderTest, test_setting_multiple_steps_1) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  builder.withDSN("testDSN").withServer("testServer").withPort(3306);

  builder.withUID("testUser").withPWD("testPwd").withLogQuery(true);
  const std::string connection_string = builder.build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder initially without all required values. Then append other properties to the builder.
// After second round of values, all required values are set. Build the connection string. Build will succeed.
TEST_F(ConnectionStringBuilderTest, test_setting_multiple_steps_2) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  builder.withDSN("testDSN").withServer("testServer").withUID("testUser").withPWD("testPwd");

  builder.withPort(3306).withLogQuery(true);
  const std::string connection_string = builder.build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder with some values. Then append more values to the builder, but leaving required values unset.
// Then build the connection string. Error expected.
TEST_F(ConnectionStringBuilderTest, test_multiple_steps_without_required) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  builder.withServer("testServer").withPort(3306);
  EXPECT_THROW(builder.withUID("testUser").withPWD("testPwd").withLogQuery(true).build(), std::runtime_error);
}
