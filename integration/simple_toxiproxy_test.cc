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

#include <sql.h>
#include <sqlext.h>
#include <gtest/gtest.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

static SQLHENV env;
static SQLHDBC dbc;
class NetworkFailoverIntegrationTest : public testing::Test
{
public:
  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");
  char* server;

  SQLCHAR connIn[4096], connOut[4096], sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native_error;
  SQLSMALLINT len, length;

  char* PROXIED_DOMAIN_NAME_SUFFIX = std::getenv("PROXIED_DOMAIN_NAME_SUFFIX");
  char* PROXIED_CLUSTER_TEMPLATE = std::getenv("PROXIED_CLUSTER_TEMPLATE");
  char* DB_CONN_STR_SUFFIX = std::getenv("DB_CONN_STR_SUFFIX");

  std::string MYSQL_INSTANCE_1_URL = std::getenv("MYSQL_INSTANCE_1_URL");
  std::string MYSQL_INSTANCE_2_URL = std::getenv("MYSQL_INSTANCE_2_URL");
  std::string MYSQL_INSTANCE_3_URL = std::getenv("MYSQL_INSTANCE_3_URL");
  std::string MYSQL_INSTANCE_4_URL = std::getenv("MYSQL_INSTANCE_4_URL");
  std::string MYSQL_INSTANCE_5_URL = std::getenv("MYSQL_INSTANCE_5_URL");
  char* MYSQL_CLUSTER_URL = std::getenv("TEST_SERVER");
  char* MYSQL_RO_CLUSTER_URL = std::getenv("TEST_RO_SERVER");

  int MYSQL_PORT = atoi(std::getenv("MYSQL_PORT"));
  int MYSQL_PROXY_PORT = atoi(std::getenv("MYSQL_PROXY_PORT"));

  char* TOXIPROXY_INSTANCE_1_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_1_NETWORK_ALIAS");
  char* TOXIPROXY_INSTANCE_2_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_2_NETWORK_ALIAS");
  char* TOXIPROXY_INSTANCE_3_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_3_NETWORK_ALIAS");
  char* TOXIPROXY_INSTANCE_4_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_4_NETWORK_ALIAS");
  char* TOXIPROXY_INSTANCE_5_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_5_NETWORK_ALIAS");
  char* TOXIPROXY_CLUSTER_NETWORK_ALIAS = std::getenv("TOXIPROXY_CLUSTER_NETWORK_ALIAS");
  char* TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS = std::getenv("TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS");
  int TOXIPROXY_CONTROL_PORT = 8474;

  TOXIPROXY_CLIENT* toxiproxy_client_instance_1 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_1_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_2 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_2_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_3 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_3_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_4 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_4_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT* toxiproxy_client_instance_5 = new TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_5_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT *toxiproxy_cluster = new TOXIPROXY_CLIENT(TOXIPROXY_CLUSTER_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);
  TOXIPROXY_CLIENT *toxiproxy_read_only_cluster = new TOXIPROXY_CLIENT(TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS, TOXIPROXY_CONTROL_PORT);

  PROXY* proxy_instance_1 = get_proxy(toxiproxy_client_instance_1, MYSQL_INSTANCE_1_URL, MYSQL_PORT);
  PROXY* proxy_instance_2 = get_proxy(toxiproxy_client_instance_2, MYSQL_INSTANCE_2_URL, MYSQL_PORT);
  PROXY* proxy_instance_3 = get_proxy(toxiproxy_client_instance_3, MYSQL_INSTANCE_3_URL, MYSQL_PORT);
  PROXY* proxy_instance_4 = get_proxy(toxiproxy_client_instance_4, MYSQL_INSTANCE_4_URL, MYSQL_PORT);
  PROXY* proxy_instance_5 = get_proxy(toxiproxy_client_instance_5, MYSQL_INSTANCE_5_URL, MYSQL_PORT);
  PROXY *proxy_cluster = get_proxy(toxiproxy_cluster, MYSQL_CLUSTER_URL, MYSQL_PORT);
  PROXY *proxy_read_only_cluster = get_proxy(toxiproxy_read_only_cluster, MYSQL_RO_CLUSTER_URL, MYSQL_PORT);

  std::map<std::string, PROXY*> proxy_map = {
      {MYSQL_INSTANCE_1_URL.substr(0, MYSQL_INSTANCE_1_URL.find('.')), proxy_instance_1},
      {MYSQL_INSTANCE_2_URL.substr(0, MYSQL_INSTANCE_2_URL.find('.')), proxy_instance_2},
      {MYSQL_INSTANCE_3_URL.substr(0, MYSQL_INSTANCE_3_URL.find('.')), proxy_instance_3},
      {MYSQL_INSTANCE_4_URL.substr(0, MYSQL_INSTANCE_4_URL.find('.')), proxy_instance_4},
      {MYSQL_INSTANCE_5_URL.substr(0, MYSQL_INSTANCE_5_URL.find('.')), proxy_instance_5},
      {MYSQL_CLUSTER_URL, proxy_cluster},
      {MYSQL_RO_CLUSTER_URL, proxy_read_only_cluster}
  };

  static void SetUpTestSuite()
  {
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
  }

  static void TearDownTestSuite()
  {
    if (nullptr != dbc)
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);

    if (nullptr != env)
      SQLFreeHandle(SQL_HANDLE_ENV, env);

    // for (auto const x : proxy_map)
    // {
    // 	delete x.second;
    // }
  }

  void SetUp() override
  {
    // for (auto const x : proxy_map)
    // {
    // 	enable_connectivity(x.second);
    // }
  }

  PROXY* get_proxy(TOXIPROXY_CLIENT* client, const std::string& host, int port) const
  {
    const std::string upstream = host + ":" + std::to_string(port);
    return client->get_proxy(upstream);
  }

  void disable_connectivity(PROXY* proxy)
  {
    const auto toxics = proxy->get_toxics();
    toxics->bandwidth("DOWN-STREAM", TOXIC_DIRECTION::DOWNSTREAM, 0);
    toxics->bandwidth("UP-STREAM", TOXIC_DIRECTION::UPSTREAM, 0);
  }

  void enable_connectivity(PROXY* proxy)
  {
    const auto toxics = proxy->get_toxics();
    // TODO: handle empty toxic lists appropriately
    TOXIC* downstream = toxics->get("DOWN-STREAM");
    TOXIC* upstream = toxics->get("UP-STREAM");

    if (downstream)
      downstream->remove();

    if (upstream)
      upstream->remove();
  }
};

TEST_F(NetworkFailoverIntegrationTest, ToxiproxyConnectionTest)
{
  const std::string server = MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX;
  sprintf(reinterpret_cast<char*>(connIn), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;HOST_PATTERN=%s;", dsn, user, pwd, server.c_str(), MYSQL_PROXY_PORT, PROXIED_CLUSTER_TEMPLATE);

  const SQLRETURN rc = SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
  SQLRETURN drc = SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &length);

  if (SQL_SUCCEEDED(drc))
    printf("# [%6s] %*sd\n", sqlstate, length, message);

  EXPECT_EQ(SQL_SUCCESS, rc);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
