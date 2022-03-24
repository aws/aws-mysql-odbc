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

// This include order matters on Windows
#include <aws/rds/model/TargetState.h>
#include <aws/rds/model/TargetHealth.h>

#include <aws/rds/RDSClient.h>
#include "toxiproxy/toxiproxy_client.h"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/rds/model/DBCluster.h>
#include <aws/rds/model/DBClusterMember.h>
#include <aws/rds/model/DescribeDBClustersRequest.h>

#include <gtest/gtest.h>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 4096
#define SQL_MAX_MESSAGE_LENGTH 512
static const std::string DOWN_STREAM_STR = "DOWNSTREAM";
static const std::string UP_STREAM_STR = "UPSTREAM";
static SQLHENV env;
static SQLHDBC dbc;
static Aws::SDKOptions options;

class NetworkFailoverIntegrationTest : public testing::Test {
 public:
  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");

  std::vector<Aws::RDS::Model::DBClusterMember> readers;
  Aws::RDS::Model::DBClusterMember writer;

  SQLCHAR connIn[4096], connOut[4096], sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native_error;
  SQLSMALLINT len, length;

  std::string PROXIED_DOMAIN_NAME_SUFFIX = std::getenv("PROXIED_DOMAIN_NAME_SUFFIX");
  std::string PROXIED_CLUSTER_TEMPLATE = std::getenv("PROXIED_CLUSTER_TEMPLATE");
  std::string DB_CONN_STR_SUFFIX = std::getenv("DB_CONN_STR_SUFFIX");

  std::string MYSQL_INSTANCE_1_URL = std::getenv("MYSQL_INSTANCE_1_URL");
  std::string MYSQL_INSTANCE_2_URL = std::getenv("MYSQL_INSTANCE_2_URL");
  std::string MYSQL_INSTANCE_3_URL = std::getenv("MYSQL_INSTANCE_3_URL");
  std::string MYSQL_INSTANCE_4_URL = std::getenv("MYSQL_INSTANCE_4_URL");
  std::string MYSQL_INSTANCE_5_URL = std::getenv("MYSQL_INSTANCE_5_URL");
  std::string MYSQL_CLUSTER_URL = std::getenv("TEST_SERVER");
  std::string MYSQL_RO_CLUSTER_URL = std::getenv("TEST_RO_SERVER");

  int MYSQL_PORT = atoi(std::getenv("MYSQL_PORT"));
  int MYSQL_PROXY_PORT = atoi(std::getenv("MYSQL_PROXY_PORT"));

  std::string TOXIPROXY_INSTANCE_1_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_1_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_2_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_2_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_3_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_3_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_4_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_4_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_5_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_5_NETWORK_ALIAS");
  std::string TOXIPROXY_CLUSTER_NETWORK_ALIAS = std::getenv("TOXIPROXY_CLUSTER_NETWORK_ALIAS");
  std::string TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS = std::getenv("TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS");
  int TOXIPROXY_CONTROL_PORT = 8474;

  TOXIPROXY_CLIENT* toxiproxy_client_instance_1 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_1_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_2 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_2_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_3 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_3_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_4 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_4_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_5 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_5_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_cluster = new TOXIPROXY_CLIENT(TOXIPROXY_CLUSTER_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_read_only_cluster = new TOXIPROXY_CLIENT(TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);

  PROXY* proxy_instance_1 = get_proxy(toxiproxy_client_instance_1, MYSQL_INSTANCE_1_URL, MYSQL_PORT);
  PROXY* proxy_instance_2 = get_proxy(toxiproxy_client_instance_2, MYSQL_INSTANCE_2_URL, MYSQL_PORT);
  PROXY* proxy_instance_3 = get_proxy(toxiproxy_client_instance_3, MYSQL_INSTANCE_3_URL, MYSQL_PORT);
  PROXY* proxy_instance_4 = get_proxy(toxiproxy_client_instance_4, MYSQL_INSTANCE_4_URL, MYSQL_PORT);
  PROXY* proxy_instance_5 = get_proxy(toxiproxy_client_instance_5, MYSQL_INSTANCE_5_URL, MYSQL_PORT);
  PROXY* proxy_cluster = get_proxy(toxiproxy_cluster, MYSQL_CLUSTER_URL, MYSQL_PORT);
  PROXY* proxy_read_only_cluster = get_proxy(toxiproxy_read_only_cluster, MYSQL_RO_CLUSTER_URL, MYSQL_PORT);

  std::map<std::string, PROXY*> proxy_map = {{MYSQL_INSTANCE_1_URL.substr(0, MYSQL_INSTANCE_1_URL.find('.')), proxy_instance_1},
                                             {MYSQL_INSTANCE_2_URL.substr(0, MYSQL_INSTANCE_2_URL.find('.')), proxy_instance_2},
                                             {MYSQL_INSTANCE_3_URL.substr(0, MYSQL_INSTANCE_3_URL.find('.')), proxy_instance_3},
                                             {MYSQL_INSTANCE_4_URL.substr(0, MYSQL_INSTANCE_4_URL.find('.')), proxy_instance_4},
                                             {MYSQL_INSTANCE_5_URL.substr(0, MYSQL_INSTANCE_5_URL.find('.')), proxy_instance_5},
                                             {MYSQL_CLUSTER_URL, proxy_cluster},
                                             {MYSQL_RO_CLUSTER_URL, proxy_read_only_cluster}};

  static void SetUpTestSuite() {
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    Aws::InitAPI(options); 
  }

  static void TearDownTestSuite() {
    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    };
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
    Aws::ShutdownAPI(options); 
  }

  void SetUp() override {
    for (const auto& x : proxy_map) {
      enable_connectivity(x.second);
    }

    get_topology();
  }

  PROXY* get_proxy(TOXIPROXY_CLIENT* client, const std::string& host, int port) const {
    const std::string upstream = host + ":" + std::to_string(port);
    return client->get_proxy(upstream);
  }

  PROXY* get_proxy_from_map(const std::string& url) {
    const auto it = proxy_map.find(url);
    if (it != proxy_map.end()) {
      return it->second;
    }
    return nullptr;
  }

  void get_topology() {
    Aws::Client::ClientConfiguration clientConfig;
    clientConfig.region = "us-east-2";

    Aws::Auth::AWSCredentials credentials;
    credentials.SetAWSAccessKeyId(Aws::String(ACCESS_KEY));
    credentials.SetAWSSecretKey(Aws::String(SECRET_ACCESS_KEY));

    Aws::String cluster_id(MYSQL_CLUSTER_URL.substr(0, MYSQL_CLUSTER_URL.find('.')));
    Aws::RDS::RDSClient client(credentials, clientConfig);
    Aws::RDS::Model::DescribeDBClustersRequest rds_req;
    rds_req.WithDBClusterIdentifier(cluster_id);
    auto outcome = client.DescribeDBClusters(rds_req);

    if (!outcome.IsSuccess()) {
      FAIL() << "Error describing cluster. ";
    }

    auto result = outcome.GetResult();
    const Aws::Vector<Aws::RDS::Model::DBCluster> clusters = result.GetDBClusters();
    for (const auto& cluster : clusters)  // Only one cluster in the vector
    {
      for (const auto& instance : cluster.GetDBClusterMembers()) {
        if (instance.GetIsClusterWriter()) {
          writer = instance;
        } else {
          readers.push_back(instance);
        }
      }
    }
  }

  std::string get_writer_id() { return writer.GetDBInstanceIdentifier(); }

  std::string get_reader_id() {
    const std::string reader_id = readers[0].GetDBInstanceIdentifier();
    return reader_id;
  }

  bool is_db_instance_writer(const std::string instance) {
    const std::string writer_instance = writer.GetDBInstanceIdentifier();
    return (writer.GetDBInstanceIdentifier() == instance);
  }

  bool is_db_instance_reader(const std::string instance) {
    for (const auto& reader : readers) {
      if (reader.GetDBInstanceIdentifier() == instance) {
        return true;
      }
    }
    return false;
  }

  std::string get_endpoint(const std::string instance_id) const { return instance_id + DB_CONN_STR_SUFFIX + PROXIED_DOMAIN_NAME_SUFFIX; }

  std::string get_current_instance_id() const {
    SQLCHAR buf[255];
    SQLLEN buflen;
    SQLHSTMT handle;

    const auto query = (SQLCHAR*)"SELECT @@aurora_server_id";
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle));
    EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    std::string ret = (char*)buf;
    return ret;
  }

  void disable_instance(const std::string instance) {
    PROXY* new_instance = get_proxy_from_map(instance);
    if (new_instance) {
      disable_connectivity(new_instance);
    } else {
      FAIL() << instance << " does not have a proxy setup.";
    }
  }

  static void disable_connectivity(const PROXY* proxy) {
    const auto toxics = proxy->get_toxics();
    if (toxics) {
      toxics->bandwidth(DOWN_STREAM_STR, TOXIC_DIRECTION::DOWNSTREAM, 0);
      toxics->bandwidth(UP_STREAM_STR, TOXIC_DIRECTION::UPSTREAM, 0);
    }
  }

  void enable_instance(const std::string instance) {
    PROXY* new_instance = get_proxy_from_map(instance);
    if (new_instance) {
      enable_connectivity(new_instance);
    } else {
      FAIL() << instance << " does not have a proxy setup.";
    }
  }

  static void enable_connectivity(const PROXY* proxy) {
    TOXIC_LIST* toxics = proxy->get_toxics();

    if (toxics) {
      TOXIC* downstream = toxics->get(DOWN_STREAM_STR);
      TOXIC* upstream = toxics->get(UP_STREAM_STR);

      if (downstream) {
        downstream->remove();
      }

      if (upstream) {
        upstream->remove();
      }
    }
  }

  std::string get_default_config(int connect_timeout = 10, int network_timeout = 10) const {
    char template_connection[4096];
    sprintf(template_connection, "DSN=%s;UID=%s;PWD=%s;LOG_QUERY=1;CONNECT_TIMEOUT=%d;NETWORK_TIMEOUT=%d;", dsn, user, pwd, connect_timeout, network_timeout);
    std::string config(template_connection);
    return config;
  }

  std::string get_default_proxied_config() const {
    char template_connection[4096];
    sprintf(template_connection, "%sHOST_PATTERN=%s;", get_default_config().c_str(), PROXIED_CLUSTER_TEMPLATE.c_str());
    std::string config(template_connection);
    return config;
  }

  void test_connection(const std::string test_server, int test_port) {
    sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), test_server.c_str(), test_port);

    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
  }

  void assert_query_succeeded() const {
    const auto query = (SQLCHAR*)"SELECT @@aurora_server_id";
    SQLHSTMT handle;

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
  }

  void assert_query_failed(const std::string expected_error) const {
    SQLHSTMT handle;
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;
    SQLCHAR message[SQL_MAX_MESSAGE_LENGTH], stmt_sqlstate[6];
    const auto query = (SQLCHAR*)"SELECT @@aurora_server_id";

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_ERROR, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLError(env, dbc, handle, stmt_sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
    const std::string state = (char*)stmt_sqlstate;
    EXPECT_EQ(expected_error, state);
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
  }

  void assert_is_new_reader(std::vector<std::string> old_readers, std::string new_reader) const {
    for (const auto& reader : old_readers) {
      EXPECT_NE(reader, new_reader);
    }
  }
};

TEST_F(NetworkFailoverIntegrationTest, connection_test) {
  test_connection(MYSQL_INSTANCE_1_URL, MYSQL_PORT);
  test_connection(MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
  test_connection(MYSQL_CLUSTER_URL, MYSQL_PORT);
  test_connection(MYSQL_CLUSTER_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
  test_connection(MYSQL_RO_CLUSTER_URL, MYSQL_PORT);
  test_connection(MYSQL_RO_CLUSTER_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_writer) {
  const std::string writer_id = get_writer_id();
  const std::string server = get_endpoint(writer_id);

  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  assert_query_succeeded();

  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }
  disable_connectivity(proxy_cluster);

  const std::string expected = "08S01";
  assert_query_failed(expected);

  enable_connectivity(writer_proxy);
  enable_connectivity(proxy_cluster);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_all_readers) {
  const std::string writer_id = get_writer_id();
  const std::string reader_id = get_reader_id();
  const std::string server = get_endpoint(reader_id);

  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  for (const auto& x : proxy_map) {
    if (x.first != writer_id) disable_connectivity(x.second);
  }

  const std::string expected = "08S02";
  assert_query_failed(expected);

  const std::string new_reader_id = get_current_instance_id();
  EXPECT_EQ(writer_id, new_reader_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_reader_instance) {
  const std::string writer_id = get_writer_id();
  const std::string reader_id = get_reader_id();
  const std::string server = get_endpoint(reader_id);

  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(reader_id);

  const std::string expected = "08S02";
  assert_query_failed(expected);

  const std::string new_instance = get_current_instance_id();
  EXPECT_EQ(writer_id, new_instance);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_read_only) {
  const std::string writer_id = get_writer_id();
  const std::string reader_id = get_reader_id();
  const std::string server = get_endpoint(reader_id);

  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;ALLOW_READER_CONNECTIONS=1;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(reader_id);

  const std::string expected = "08S02";
  assert_query_failed(expected);

  const std::string new_reader_id = get_current_instance_id();
  EXPECT_NE(writer_id, new_reader_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, writer_connection_fails_due_to_no_reader) {
  const char* writer_id = get_writer_id().c_str();
  const std::string server = MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX;

  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  // Put all but writer down first
  for (const auto& x : proxy_map) {
    if (x.first != writer_id) disable_connectivity(x.second);
  }

  // Crash the writer now
  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }

  const std::string expected = "08S01";
  assert_query_failed(expected);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, fail_from_reader_to_reader_with_some_readers_are_down) {
  // Assert there are at least 2 readers in the cluster.
  EXPECT_LE(2, readers.size());

  const std::string writer_id = get_writer_id();
  const std::string reader_id = get_reader_id();
  const std::string server = get_endpoint(reader_id);
  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;ALLOW_READER_CONNECTIONS=1;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  for (size_t index = 0; index < readers.size() - 1; ++index) {
    disable_instance(readers[index].GetDBInstanceIdentifier());
  }

  const std::string expected = "08S02";
  assert_query_failed(expected);

  const std::string current_connection = get_current_instance_id();
  const std::string last_reader = readers.back().GetDBInstanceIdentifier();

  // Assert that new instance is either the last reader instance or the writer instance.
  EXPECT_TRUE(current_connection == last_reader || current_connection == writer_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, failover_back_to_the_previously_down_reader) {
  // Assert there are at least 4 readers in the cluster.
  EXPECT_LE(4, readers.size());

  std::vector<std::string> previous_readers;
  const std::string expected_error = "08S02";

  const std::string writer = get_writer_id();
  const std::string first_reader = get_reader_id();
  const std::string server = get_endpoint(first_reader);
  previous_readers.push_back(first_reader);

  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;ALLOW_READER_CONNECTIONS=1;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(first_reader);
  assert_query_failed(expected_error);

  const std::string second_reader = get_current_instance_id();
  EXPECT_TRUE(is_db_instance_reader(second_reader));
  assert_is_new_reader(previous_readers, second_reader);
  previous_readers.push_back(second_reader);

  disable_instance(second_reader);
  assert_query_failed(expected_error);

  const std::string third_reader = get_current_instance_id();
  EXPECT_TRUE(is_db_instance_reader(third_reader));
  assert_is_new_reader(previous_readers, third_reader);
  previous_readers.push_back(third_reader);

  // Find the fourth reader instance
  std::string last_reader;
  for (const auto& reader : readers) {
    const std::string reader_id = reader.GetDBInstanceIdentifier();
    bool is_same = false;

    for (const auto& used_reader : previous_readers) {
      if (used_reader == reader_id) {
        is_same = true;
        break;
      }
    }

    if (is_same) {
      continue;
    }

    last_reader = reader_id;
  }

  assert_is_new_reader(previous_readers, last_reader);

  // Crash the fourth reader instance.
  disable_instance(last_reader);

  // Stop crashing the first and second.
  enable_instance(previous_readers[0]);
  enable_instance(previous_readers[1]);

  const std::string current_instance_id = get_current_instance_id();
  EXPECT_EQ(third_reader, current_instance_id);

  // Start crashing the third instance.
  disable_instance(third_reader);

  assert_query_failed(expected_error);

  const std::string last_instance_id = get_current_instance_id();

  // Assert that the last instance is either the first reader instance or the second reader instance.
  EXPECT_TRUE(last_instance_id == first_reader || last_instance_id == second_reader);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
