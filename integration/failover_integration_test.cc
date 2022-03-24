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

#include <aws/rds/model/TargetState.h>
#include <aws/rds/model/TargetHealth.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/rds/RDSClient.h>
#include <aws/rds/model/DBClusterMember.h>
#include <aws/rds/model/DBCluster.h>
#include <aws/rds/model/DescribeDBClustersRequest.h>
#include <aws/rds/model/FailoverDBClusterRequest.h>

#include <cstdlib>
#include <stdexcept>
#include <sql.h>
#include <sqlext.h>
#include <gtest/gtest.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

static SQLHENV env;
static SQLHDBC dbc;
static Aws::SDKOptions options;

class FailoverIntegrationTest : public testing::Test {
protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");

  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");

  std::string MYSQL_CLUSTER_URL = std::getenv("TEST_SERVER");
  std::string MYSQL_RO_CLUSTER_URL = std::getenv("TEST_RO_SERVER");
  std::string DB_CONN_STR_SUFFIX = std::getenv("DB_CONN_STR_SUFFIX");

  int MYSQL_PORT = atoi(std::getenv("MYSQL_PORT"));
  Aws::String cluster_id = MYSQL_CLUSTER_URL.substr(0, MYSQL_CLUSTER_URL.find('.'));

  SQLCHAR conn_in[4096];

  static void SetUpTestSuite() {
    Aws::InitAPI(options);
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
  }

  static void TearDownTestSuite() {
    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
    Aws::ShutdownAPI(options);
  }

  void SetUp() override {

  }

  void TearDown() override {  
  }
};

// Helper functions

void build_connection_string(SQLCHAR* conn_in, char* dsn, char* user, char* pwd, std::string server, int port) {
  sprintf(reinterpret_cast<char*>(conn_in), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;LOG_QUERY=1;", dsn, user, pwd, server.c_str(), port);
}

std::vector<std::string> retrieve_topology_via_SQL(SQLCHAR* conn_in) {
  std::vector<std::string> instances;

  SQLCHAR conn_out[4096], sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native_error;
  SQLSMALLINT len, length;

  SQLRETURN rc = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    throw "SQLDriverConnect failed.";
  }

  SQLCHAR buf[255];
  SQLLEN buflen;
  SQLHSTMT handle;
  const auto query = (SQLCHAR*)"SELECT SERVER_ID FROM information_schema.replica_host_status ORDER BY IF(SESSION_ID = 'MASTER_SESSION_ID', 0, 1)";

  SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);
  SQLExecDirect(handle, query, SQL_NTS);
  auto rc_fetch = SQLFetch(handle);

  if (rc_fetch == SQL_NO_DATA) {
    return instances;
  }

  while (rc_fetch != SQL_NO_DATA_FOUND) {
    if (rc_fetch == SQL_ERROR) { 
        break;
    }
    rc_fetch = SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen);
    std::string instance_id((const char*)buf);
    instances.push_back(instance_id);
    rc_fetch = SQLFetch(handle);
  }

  SQLFreeHandle(SQL_HANDLE_STMT, handle);
  SQLDisconnect(dbc);
  return instances;
}

std::string query_instance_id(SQLHDBC dbc) {
  SQLCHAR buf[255];
  SQLLEN buflen;
  SQLHSTMT handle;
  const auto query = (SQLCHAR*)"SELECT @@aurora_server_id";

  SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);
  SQLExecDirect(handle, query, SQL_NTS);
  const auto rc = SQLFetch(handle);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    throw "Unable to fetch the instance id";
  }

  SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen);
  SQLFreeHandle(SQL_HANDLE_STMT, handle);

  std::string id((const char*)buf);
  return id;
}

std::vector<std::string> retrieve_topology_via_SDK(Aws::RDS::RDSClient client, Aws::String cluster_id) {
  std::vector<std::string> instances;

  std::string writer;
  std::vector<std::string> readers;

  Aws::RDS::Model::DescribeDBClustersRequest rds_req;
  rds_req.WithDBClusterIdentifier(cluster_id);
  auto outcome = client.DescribeDBClusters(rds_req);

  if (!outcome.IsSuccess()) {
    return instances;
  }
 
  const auto result = outcome.GetResult();
  const Aws::RDS::Model::DBCluster cluster = result.GetDBClusters()[0];

  for (const auto& instance : cluster.GetDBClusterMembers()) {
    std::string instance_id(instance.GetDBInstanceIdentifier());
    if (instance.GetIsClusterWriter()) {
      writer = instance_id;
    } else {
      readers.push_back(instance_id);
    }
  }

  instances.push_back(writer);
  for (const auto& reader : readers) {
    instances.push_back(reader);
  }
  return instances;
}

std::pair<std::string, std::string> retrieve_writer_endpoint(Aws::RDS::RDSClient client,
                                                             Aws::String cluster_id,
                                                             std::string endpoint_suffix) {
  std::vector<std::string> instances = retrieve_topology_via_SDK(client, cluster_id);
  if (instances.empty()) {
    throw std::runtime_error("Unable to fetch cluster instances.");
  }
  const std::string writer_id = instances[0];
  auto instance = std::make_pair(writer_id, writer_id + endpoint_suffix);
  return instance;
}

Aws::RDS::Model::DBCluster get_DB_cluster(Aws::RDS::RDSClient client, Aws::String cluster_id) {
  Aws::RDS::Model::DescribeDBClustersRequest rds_req;
  rds_req.WithDBClusterIdentifier(cluster_id);
  auto outcome = client.DescribeDBClusters(rds_req);
  auto result = outcome.GetResult();
  return result.GetDBClusters().at(0);
}

void wait_until_cluster_has_right_state(Aws::RDS::RDSClient client, Aws::String cluster_id) {
  Aws::String status = get_DB_cluster(client, cluster_id).GetStatus();

  while (status != "available") {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    status = get_DB_cluster(client, cluster_id).GetStatus();
  }
}

Aws::RDS::Model::DBClusterMember get_DB_cluster_writer_instance(Aws::RDS::RDSClient client, Aws::String cluster_id) {
  Aws::RDS::Model::DBClusterMember instance;
  Aws::RDS::Model::DBCluster cluster = get_DB_cluster(client, cluster_id);
  for (const auto& member : cluster.GetDBClusterMembers()) {
    if (member.GetIsClusterWriter()) {
      return member;
    }
  }
  return instance;
}

Aws::String get_DB_cluster_writer_instance_id(Aws::RDS::RDSClient client, Aws::String cluster_id) {
  return get_DB_cluster_writer_instance(client, cluster_id).GetDBInstanceIdentifier();
}

void wait_until_writer_instance_changed(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String initial_writer_instance_id) {
  Aws::String next_cluster_writer_id = get_DB_cluster_writer_instance_id(client, cluster_id);
  while (initial_writer_instance_id == next_cluster_writer_id) {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    next_cluster_writer_id = get_DB_cluster_writer_instance_id(client, cluster_id);
  }
}

void failover_cluster(Aws::RDS::RDSClient client, Aws::String cluster_id) {
  wait_until_cluster_has_right_state(client, cluster_id);
  Aws::RDS::Model::FailoverDBClusterRequest rds_req;
  rds_req.WithDBClusterIdentifier(cluster_id);
  auto outcome = client.FailoverDBCluster(rds_req);
}

void failover_cluster_and_wait_until_writer_changed(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String cluster_writer_id) {
  failover_cluster(client, cluster_id);
  wait_until_writer_instance_changed(client, cluster_id, cluster_writer_id);
}

Aws::RDS::Model::DBClusterMember get_matched_DBClusterMember(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String instance_id) {
  Aws::RDS::Model::DBClusterMember instance;
  Aws::RDS::Model::DBCluster cluster = get_DB_cluster(client, cluster_id);
  for (const auto& member : cluster.GetDBClusterMembers()) {
    Aws::String member_id = member.GetDBInstanceIdentifier();
    if (member_id == instance_id) {
      return member;
    }
  }
  return instance;
}

bool is_DB_instance_writer(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String instance_id) {
    return get_matched_DBClusterMember(client, cluster_id, instance_id).GetIsClusterWriter();
}

bool is_DB_instance_reader(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String instance_id) {
    return !get_matched_DBClusterMember(client, cluster_id, instance_id).GetIsClusterWriter();
}

// Tests

/**
* Current writer dies, a reader instance is nominated to be a new writer, failover to the new
* writer. Driver failover occurs when executing a method against the connection
*/
TEST_F(FailoverIntegrationTest, test_failFromWriterToNewWriter_failOnConnectionInvocation) {
  Aws::Client::ClientConfiguration clientConfig;
  clientConfig.region = "us-east-2";
  Aws::Auth::AWSCredentials credentials;
  credentials.SetAWSAccessKeyId(Aws::String(ACCESS_KEY));
  credentials.SetAWSSecretKey(Aws::String(SECRET_ACCESS_KEY));
  Aws::RDS::RDSClient client(credentials, clientConfig);

  auto initial_writer = retrieve_writer_endpoint(client, cluster_id, DB_CONN_STR_SUFFIX);
  auto initial_writer_id = initial_writer.first;
  auto initial_writer_endpoint = initial_writer.second;
  
  build_connection_string(conn_in, dsn, user, pwd, initial_writer_endpoint, MYSQL_PORT);
  SQLCHAR conn_out[4096], sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native_error;
  SQLSMALLINT len, length;

  SQLRETURN rc = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    FAIL();
  }

  failover_cluster_and_wait_until_writer_changed(client, cluster_id, initial_writer_id);

  SQLCHAR buf2[255];
  SQLLEN buflen2;
  SQLHSTMT handle2;
  SQLSMALLINT stmt_length;
  SQLCHAR stmt_sqlstate[6];
  const auto query2 = (SQLCHAR*)"SELECT CONCAT(@@hostname, ':', @@port)";

  EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle2));
  EXPECT_EQ(SQL_ERROR, SQLExecDirect(handle2, query2, SQL_NTS));
  EXPECT_EQ(SQL_SUCCESS, SQLError(env, dbc, handle2, stmt_sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));

  const std::string state = (char*)stmt_sqlstate;
  const std::string expected = "08S02";
  EXPECT_EQ(expected, state);
  
  std::string current_connection_id = query_instance_id(dbc);
  EXPECT_TRUE(is_DB_instance_writer(client, cluster_id, current_connection_id));
  EXPECT_NE(current_connection_id, initial_writer_id);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(FailoverIntegrationTest, EndToEndTest) {
  Aws::Client::ClientConfiguration clientConfig;
  clientConfig.region = "us-east-2";
  Aws::Auth::AWSCredentials credentials;
  credentials.SetAWSAccessKeyId(Aws::String(ACCESS_KEY));
  credentials.SetAWSSecretKey(Aws::String(SECRET_ACCESS_KEY));
  Aws::RDS::RDSClient client(credentials, clientConfig);

  std::string initial_writer = retrieve_writer_endpoint(client, cluster_id, DB_CONN_STR_SUFFIX).second;

  build_connection_string(conn_in, dsn, user, pwd, initial_writer, MYSQL_PORT);
  SQLCHAR conn_out[4096], sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native_error;
  SQLSMALLINT len, length;

  SQLRETURN rc = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    FAIL();
  }

  SQLCHAR buf[255];
  SQLLEN buflen;
  SQLHSTMT handle;
  const auto query = (SQLCHAR*)"SELECT CONCAT(@@hostname, ':', @@port)";

  EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
  EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
  EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle));
  EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));

  failover_cluster(client, cluster_id);
  std::this_thread::sleep_for(std::chrono::seconds(90));

  SQLCHAR buf2[255];
  SQLLEN buflen2;
  SQLHSTMT handle2;
  SQLSMALLINT stmt_length;
  SQLCHAR stmt_sqlstate[6];
  const auto query2 = (SQLCHAR*)"SELECT CONCAT(@@hostname, ':', @@port)";

  EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle2));
  EXPECT_EQ(SQL_ERROR, SQLExecDirect(handle2, query2, SQL_NTS));
  EXPECT_EQ(SQL_SUCCESS, SQLError(env, dbc, handle2, stmt_sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));

  const std::string state = (char*)stmt_sqlstate;
  const std::string expected = "08S02";
  EXPECT_EQ(expected, state);

  SQLCHAR buf3[255];
  SQLLEN buflen3;
  SQLHSTMT handle3;
  const auto query3 = (SQLCHAR*)"SELECT CONCAT(@@hostname, ':', @@port)";

  EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle3));
  EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle3, query3, SQL_NTS));
  EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle3));
  EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle3, 1, SQL_CHAR, buf3, sizeof(buf3), &buflen3));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle3));

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
