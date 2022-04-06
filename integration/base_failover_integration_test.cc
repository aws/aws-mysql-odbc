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

// Those classes need to be included first in Windows
#include <aws/rds/model/TargetState.h>
#include <aws/rds/model/TargetHealth.h>

#include <aws/rds/RDSClient.h>
#include "toxiproxy/toxiproxy_client.h"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/rds/model/DBCluster.h>
#include <aws/rds/model/DBClusterMember.h>
#include <aws/rds/model/DescribeDBClustersRequest.h>
#include <aws/rds/model/FailoverDBClusterRequest.h>

#include <gtest/gtest.h>
#include <cstdlib>
#include <stdexcept>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

static std::string DOWN_STREAM_STR = "DOWNSTREAM";
static std::string UP_STREAM_STR = "UPSTREAM";

static SQLHENV env;
static SQLHDBC dbc;
static Aws::SDKOptions options;

class BaseFailoverIntegrationTest : public testing::Test {
protected:
  // AWS credentials
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");

  // Connection string parameters
  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");

  std::string MYSQL_INSTANCE_1_URL = std::getenv("MYSQL_INSTANCE_1_URL");
  std::string MYSQL_INSTANCE_2_URL = std::getenv("MYSQL_INSTANCE_2_URL");
  std::string MYSQL_INSTANCE_3_URL = std::getenv("MYSQL_INSTANCE_3_URL");
  std::string MYSQL_INSTANCE_4_URL = std::getenv("MYSQL_INSTANCE_4_URL");
  std::string MYSQL_INSTANCE_5_URL = std::getenv("MYSQL_INSTANCE_5_URL");
  std::string MYSQL_CLUSTER_URL = std::getenv("TEST_SERVER");
  std::string MYSQL_RO_CLUSTER_URL = std::getenv("TEST_RO_SERVER");

  std::string PROXIED_DOMAIN_NAME_SUFFIX = std::getenv("PROXIED_DOMAIN_NAME_SUFFIX");
  std::string PROXIED_CLUSTER_TEMPLATE = std::getenv("PROXIED_CLUSTER_TEMPLATE");
  std::string DB_CONN_STR_SUFFIX = std::getenv("DB_CONN_STR_SUFFIX");

  int MYSQL_PORT = atoi(std::getenv("MYSQL_PORT"));
  int MYSQL_PROXY_PORT = atoi(std::getenv("MYSQL_PROXY_PORT"));
  Aws::String cluster_id = MYSQL_CLUSTER_URL.substr(0, MYSQL_CLUSTER_URL.find('.'));

  SQLCHAR conn_in[4096], conn_out[4096], sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native_error;
  SQLSMALLINT len, length;

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

  std::vector<std::string> cluster_instances;
  std::string writer_id;
  std::string writer_endpoint;
  std::vector<std::string> readers;
  std::string reader_id;
  std::string reader_endpoint;

  // Helper functions

  std::string get_endpoint(const std::string instance_id) const {
    return instance_id + DB_CONN_STR_SUFFIX;
  }

  std::string get_proxied_endpoint(const std::string instance_id) const {
    return instance_id + DB_CONN_STR_SUFFIX + PROXIED_DOMAIN_NAME_SUFFIX;
  }

  std::string get_writer_id(std::vector<std::string> instances) {
    if (instances.empty()) {
      throw std::runtime_error("The input cluster topology is empty.");
    }
    return instances[0];
  }

  std::vector<std::string> get_readers(std::vector<std::string> instances) {
    const std::vector<std::string>::const_iterator first_reader = instances.begin() + 1;
    const std::vector<std::string>::const_iterator last_reader = instances.end();
    std::vector<std::string> readers(first_reader, last_reader);
    return readers;
  }

  std::string get_first_reader_id(std::vector<std::string> instances) {
    if (instances.empty()) {
      throw std::runtime_error("The input cluster topology is empty.");
    }
    return instances[1];
  }

  // Helper functions from integration tests

  void build_connection_string(SQLCHAR* conn_in, char* dsn, char* user, char* pwd, std::string server, int port, char* db) {
    sprintf(reinterpret_cast<char*>(conn_in), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;DATABASE=%s;LOG_QUERY=1;", dsn, user, pwd, server.c_str(), port, db);
  }

  std::vector<std::string> query_topology(SQLCHAR* conn_in) {
    std::vector<std::string> instances;

    SQLCHAR conn_out[4096];
    SQLSMALLINT len;
    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

    SQLCHAR buf[255];
    SQLLEN buflen;
    SQLHSTMT handle;
    const auto query = (SQLCHAR*)"SELECT SERVER_ID FROM information_schema.replica_host_status ORDER BY IF(SESSION_ID = 'MASTER_SESSION_ID', 0, 1)";

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
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

    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
    return instances;
  }

  std::string query_instance_id(SQLHDBC dbc) const {
    SQLCHAR buf[255];
    SQLLEN buflen;
    SQLHSTMT handle;
    const auto query = (SQLCHAR*)"SELECT @@aurora_server_id";

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle));
    EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));

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

  Aws::RDS::Model::DBCluster get_DB_cluster(Aws::RDS::RDSClient client, Aws::String cluster_id) {
    Aws::RDS::Model::DescribeDBClustersRequest rds_req;
    rds_req.WithDBClusterIdentifier(cluster_id);
    auto outcome = client.DescribeDBClusters(rds_req);
    const auto result = outcome.GetResult();
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
    const Aws::RDS::Model::DBCluster cluster = get_DB_cluster(client, cluster_id);
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

  void failover_cluster(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String target_instance_id = "") {
    wait_until_cluster_has_right_state(client, cluster_id);
    Aws::RDS::Model::FailoverDBClusterRequest rds_req;
    rds_req.WithDBClusterIdentifier(cluster_id);
    if (!target_instance_id.empty()) {
      rds_req.WithTargetDBInstanceIdentifier(target_instance_id);
    }
    auto outcome = client.FailoverDBCluster(rds_req);
  }

  void failover_cluster_and_wait_until_writer_changed(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String cluster_writer_id, Aws::String target_writer_id = "") {
    failover_cluster(client, cluster_id, target_writer_id);
    wait_until_writer_instance_changed(client, cluster_id, cluster_writer_id);
  }

  Aws::RDS::Model::DBClusterMember get_matched_DBClusterMember(Aws::RDS::RDSClient client, Aws::String cluster_id, Aws::String instance_id) {
    Aws::RDS::Model::DBClusterMember instance;
    const Aws::RDS::Model::DBCluster cluster = get_DB_cluster(client, cluster_id);
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

  int count_table_rows(SQLHSTMT handle, const char* table) {
    std::string select_count_query = "SELECT count(*) from ";
    select_count_query.append(table);
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, (SQLCHAR*)select_count_query.c_str(), SQL_NTS));    
    auto rc_fetch = SQLFetch(handle);

    SQLINTEGER buf = -1;
    SQLLEN buflen;
    if (rc_fetch != SQL_NO_DATA_FOUND && rc_fetch != SQL_ERROR) {
      rc_fetch = SQLGetData(handle, 1, SQL_INTEGER, &buf, sizeof(buf), &buflen);
      rc_fetch = SQLFetch(handle); // To get cursor in correct position
    }
    return buf;
  }

  // Helper functions from network integration tests

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

  bool is_db_instance_writer(const std::string instance) {
    return (writer_id == instance);
  }

  bool is_db_instance_reader(const std::string instance) {
    for (const auto& reader_id : readers) {
      if (reader_id == instance) {
        return true;
      }
    }
    return false;
  }

  void disable_instance(const std::string instance) {
    PROXY* new_instance = get_proxy_from_map(instance);
    if (new_instance) {
      disable_connectivity(new_instance);
    } else {
      FAIL() << instance << " does not have a proxy setup.";
    }
  }

  void disable_connectivity(const PROXY* proxy) {
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

  void enable_connectivity(const PROXY* proxy) {
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
    sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), test_server.c_str(), test_port);

    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));
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

