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

#include "base_failover_integration_test.cc"

class NetworkFailoverIntegrationTest : public BaseFailoverIntegrationTest {
 protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");
  Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY),
                                                                    Aws::String(SECRET_ACCESS_KEY),
                                                                    Aws::String(SESSION_TOKEN));
  Aws::Client::ClientConfiguration client_config;
  Aws::RDS::RDSClient rds_client;

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
    Aws::Client::ClientConfiguration client_config;
    client_config.region = "us-east-2";
    rds_client = Aws::RDS::RDSClient(credentials, client_config);

    for (const auto& x : proxy_map) {
      enable_connectivity(x.second);
    }

    cluster_instances = retrieve_topology_via_SDK(rds_client, cluster_id);
    writer_id = get_writer_id(cluster_instances);
    writer_endpoint = get_proxied_endpoint(writer_id);
    readers = get_readers(cluster_instances);
    reader_id = get_first_reader_id(cluster_instances);
    reader_endpoint = get_proxied_endpoint(reader_id);
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
  const std::string server = get_proxied_endpoint(writer_id);

  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  assert_query_succeeded(dbc, SERVER_ID_QUERY);

  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }
  disable_connectivity(proxy_cluster);

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_FAILURE);

  enable_connectivity(writer_proxy);
  enable_connectivity(proxy_cluster);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_all_readers) {
  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), reader_endpoint.c_str(), MYSQL_PROXY_PORT);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  for (const auto& x : proxy_map) {
    if (x.first != writer_id) {
      disable_connectivity(x.second);
    }
  }

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string new_reader_id = query_instance_id(dbc);
  EXPECT_EQ(writer_id, new_reader_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_reader_instance) {
  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), reader_endpoint.c_str(), MYSQL_PROXY_PORT);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(reader_id);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string new_instance = query_instance_id(dbc);
  EXPECT_EQ(writer_id, new_instance);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_read_only) {
  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;ALLOW_READER_CONNECTIONS=1;", get_default_proxied_config().c_str(), reader_endpoint.c_str(), MYSQL_PROXY_PORT);

  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(reader_id);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string new_reader_id = query_instance_id(dbc);
  EXPECT_NE(writer_id, new_reader_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, writer_connection_fails_due_to_no_reader) {
  const char* writer_char_id = writer_id.c_str();
  const std::string server = MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX;

  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  // Put all but writer down first
  for (const auto& x : proxy_map) {
    if (x.first != writer_char_id) {
      disable_connectivity(x.second);
    }
  }

  // Crash the writer now
  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_FAILURE);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, fail_from_reader_to_reader_with_some_readers_are_down) {
  // Assert there are at least 2 readers in the cluster.
  EXPECT_LE(2, readers.size());

  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;ALLOW_READER_CONNECTIONS=1;", get_default_proxied_config().c_str(), reader_endpoint.c_str(), MYSQL_PROXY_PORT, 120000);
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  for (size_t index = 0; index < readers.size() - 1; ++index) {
    disable_instance(readers[index]);
  }

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string current_connection = query_instance_id(dbc);
  const std::string last_reader = readers.back();

  // Assert that new instance is either the last reader instance or the writer instance.
  EXPECT_TRUE(current_connection == last_reader || current_connection == writer_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, failover_back_to_the_previously_down_reader) {
  // Assert there are at least 4 readers in the cluster.
  EXPECT_LE(4, readers.size());

  std::vector<std::string> previous_readers;

  const std::string first_reader = reader_id;
  const std::string server = get_proxied_endpoint(first_reader);
  previous_readers.push_back(first_reader);

  sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;FAILOVER_T=%d;ALLOW_READER_CONNECTIONS=1;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT, 120000);
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(first_reader);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string second_reader = query_instance_id(dbc);
  EXPECT_TRUE(is_db_instance_reader(second_reader));
  assert_is_new_reader(previous_readers, second_reader);
  previous_readers.push_back(second_reader);

  disable_instance(second_reader);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string third_reader = query_instance_id(dbc);
  EXPECT_TRUE(is_db_instance_reader(third_reader));
  assert_is_new_reader(previous_readers, third_reader);
  previous_readers.push_back(third_reader);

  // Find the fourth reader instance
  std::string last_reader;
  for (const auto& reader : readers) {
    const std::string reader_id = reader;
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

  const std::string current_instance_id = query_instance_id(dbc);
  EXPECT_EQ(third_reader, current_instance_id);

  // Start crashing the third instance.
  disable_instance(third_reader);

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string last_instance_id = query_instance_id(dbc);

  // Assert that the last instance is either the first reader instance or the second reader instance.
  EXPECT_TRUE(last_instance_id == first_reader || last_instance_id == second_reader);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
