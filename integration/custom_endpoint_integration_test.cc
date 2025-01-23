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

#include <aws/rds/model/CreateDBClusterEndpointRequest.h>
#include <aws/rds/model/DeleteDBClusterEndpointRequest.h>
#include <iostream>
#include "base_failover_integration_test.cc"

class CustomEndpointIntegrationTest : public BaseFailoverIntegrationTest {
 protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");
  std::string RDS_ENDPOINT = std::getenv("RDS_ENDPOINT");
  std::string RDS_REGION = std::getenv("RDS_REGION");
  std::string ENDPOINT_ID =
      "test-endpoint-1-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  std::string region = "us-east-2";
  Aws::RDS::RDSClientConfiguration client_config;
  Aws::RDS::RDSClient rds_client;
  SQLHENV env = nullptr;
  SQLHDBC dbc = nullptr;

  bool is_endpoint_created = false;

  static void SetUpTestSuite() { Aws::InitAPI(options); }

  static void TearDownTestSuite() { Aws::ShutdownAPI(options); }
  void SetUp() override {
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    Aws::Auth::AWSCredentials credentials =
        SESSION_TOKEN.empty() ? Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY), Aws::String(SECRET_ACCESS_KEY))
                              : Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY), Aws::String(SECRET_ACCESS_KEY),
                                                          Aws::String(SESSION_TOKEN));
    if (!RDS_REGION.empty()) {
      region = RDS_REGION;
    }
    client_config.region = region;
    if (!RDS_ENDPOINT.empty()) {
      client_config.endpointOverride = RDS_ENDPOINT;
    }
    rds_client = Aws::RDS::RDSClient(credentials, client_config);

    cluster_instances = retrieve_topology_via_SDK(rds_client, cluster_id);
    writer_id = get_writer_id(cluster_instances);
    writer_endpoint = get_endpoint(writer_id);
    readers = get_readers(cluster_instances);
    reader_id = get_first_reader_id(cluster_instances);
    reader_endpoint = get_proxied_endpoint(reader_id);
    target_writer_id = get_random_DB_cluster_reader_instance_id(readers);

    if (!is_endpoint_created) {
      const std::vector writer{writer_id};
      create_custom_endpoint(cluster_id, writer);
      wait_until_endpoint_available(cluster_id);
    }
  }

  void create_custom_endpoint(const std::string& cluster_id, const std::vector<std::string>& writer) const {
    Aws::RDS::Model::CreateDBClusterEndpointRequest rds_req;
    rds_req.SetDBClusterEndpointIdentifier(ENDPOINT_ID);
    rds_req.SetDBClusterIdentifier(cluster_id);
    rds_req.SetEndpointType("ANY");
    rds_req.SetStaticMembers(writer);
    rds_client.CreateDBClusterEndpoint(rds_req);
  }

  void delete_custom_endpoint() {
    Aws::RDS::Model::DeleteDBClusterEndpointRequest rds_req;
    rds_req.SetDBClusterEndpointIdentifier(ENDPOINT_ID);
    rds_client.DeleteDBClusterEndpoint(rds_req);
  }

  /**
   * Wait up to 5 minutes for the new custom endpoint to become unavailable.
   */
  void wait_until_endpoint_available(const std::string& cluster_id) const {
    const std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    bool is_available = false;

    Aws::String status = get_DB_cluster(rds_client, cluster_id).GetStatus();

    while (std::chrono::steady_clock::now() < end_time) {
      const auto endpoint_info = get_custom_endpoint_info(rds_client, ENDPOINT_ID);
      is_available = endpoint_info.GetStatus() == "available";
      if (is_available) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    if (!is_available) {
      throw std::runtime_error(
          "The test setup step timed out while waiting for the custom endpoint to become available: " + ENDPOINT_ID);
    }
  }

  void TearDown() override {
    delete_custom_endpoint();

    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
  }
};

TEST_F(CustomEndpointIntegrationTest, test_CustomeEndpointFailover) {
  const auto endpoint_info = get_custom_endpoint_info(rds_client, ENDPOINT_ID);

  connection_string = ConnectionStringBuilder(dsn, endpoint_info.GetEndpoint(), MYSQL_PORT)
                          .withLogQuery(true)
                          .withEnableFailureDetection(true)
                          .withUID(user)
                          .withPWD(pwd)
                          .withDatabase(db)
                          .withFailoverMode("reader or writer")
                          .withEnableCustomEndpointMonitoring(true)
                          .withCustomEndpointRegion(region)
                          .getString();
  SQLCHAR conn_out[4096] = "\0";
  SQLSMALLINT len;
  std::cout << "Establishing connection to " << connection_string.c_str() << std::endl;
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out,
                                          MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  std::cout << "Established connection to " << connection_string.c_str() << std::endl;
  const std::vector<std::string>& endpoint_members = endpoint_info.GetStaticMembers();
  const std::string current_connection_id = query_instance_id(dbc);

  EXPECT_NE(std::find(endpoint_members.begin(), endpoint_members.end(), current_connection_id), endpoint_members.end());

  std::cout << "Triggering failover from " << writer_id.c_str() << " to " << current_connection_id.c_str() << std::endl;
  failover_cluster_and_wait_until_writer_changed(
      rds_client, cluster_id, writer_id, current_connection_id == writer_id ? target_writer_id : current_connection_id);

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  std::cout << "Failover triggered" << std::endl;

  const std::string new_connection_id = query_instance_id(dbc);

  std::cout << "New connection ID " << new_connection_id.c_str() << std::endl;

  EXPECT_NE(std::find(endpoint_members.begin(), endpoint_members.end(), new_connection_id), endpoint_members.end());
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));

  if (nullptr != dbc) {
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
  }
  if (nullptr != env) {
    SQLFreeHandle(SQL_HANDLE_ENV, env);
  }
}
