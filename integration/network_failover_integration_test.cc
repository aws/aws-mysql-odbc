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

#include "toxiproxy/toxiproxy_client.h"

#include <gtest/gtest.h>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

static const std::string DOWN_STREAM_STR = "DOWNSTREAM";
static const std::string UP_STREAM_STR = "UPSTREAM";

static SQLHENV env;
static SQLHDBC dbc;

class NetworkFailoverIntegrationTest : public testing::Test {
 public:
  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");

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
  }

  static void TearDownTestSuite() {
    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    };
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
  }

  void SetUp() override {
    for (auto const x : proxy_map) {
      enable_connectivity(x.second);
    }
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

  std::string get_writer_id() {
    // TODO: get writer ID via SDK
    return "";
  }

  std::string get_reader_id() {
    // TODO: get writer ID via SDK
    return "";
  }

  std::string get_current_instance_id() const {
    SQLCHAR buf[255];
    SQLLEN buflen;
    SQLHSTMT handle;

    const auto query = (SQLCHAR*)"SELECT CONCAT(@@hostname, ':', @@port)";
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle));
    EXPECT_EQ(SQL_SUCCESS,
              SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    std::string ret;
    std::copy(buf, buf + buflen, ret.begin());
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

  std::string get_default_config() const {
    char template_connection[4096];
    sprintf(template_connection, "DSN=%s;UID=%s;PWD=%s;", dsn, user, pwd);
    std::string config(template_connection);
    return config;
  }

  std::string get_default_proxied_config() const {
    char template_connection[4096];
    sprintf(template_connection, "%sHOST_PATTERN=%s;NETWORK_TIMEOUT=2;", get_default_config().c_str(), PROXIED_CLUSTER_TEMPLATE.c_str());
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
    EXPECT_EQ(SQL_SUCCESS,
              SQLError(env, dbc, handle, stmt_sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
    const std::string state = (char*)stmt_sqlstate;
    EXPECT_EQ(expected_error, state);
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
  }
};

TEST_F(NetworkFailoverIntegrationTest, ToxiproxyConnectionTest) {
  test_connection(MYSQL_INSTANCE_1_URL, MYSQL_PORT);
  test_connection(MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
  test_connection(MYSQL_CLUSTER_URL, MYSQL_PORT);
  test_connection(MYSQL_CLUSTER_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
  test_connection(MYSQL_RO_CLUSTER_URL, MYSQL_PORT);
  test_connection(MYSQL_RO_CLUSTER_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
}

TEST_F(NetworkFailoverIntegrationTest, validate_connection_when_network_down) {
  const std::string server = MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX;
  sprintf(reinterpret_cast<char*>(connIn), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), server.c_str(), MYSQL_PROXY_PORT);
  
  EXPECT_EQ(SQL_SUCCESS,
            SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  assert_query_succeeded();

  disable_connectivity(proxy_instance_1);

  const std::string expected = "08S01";
  assert_query_failed(expected);

  enable_connectivity(proxy_instance_1);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
