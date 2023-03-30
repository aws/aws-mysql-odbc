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
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/secretsmanager/SecretsManagerClient.h>

#include "base_failover_integration_test.cc"

class SecretsManagerIntegrationTest : public BaseFailoverIntegrationTest {
protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");
  Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY),
                                                                    Aws::String(SECRET_ACCESS_KEY),
                                                                    Aws::String(SESSION_TOKEN));
  std::string SECRETS_ARN = std::getenv("SECRETS_ARN");
  SQLHENV env = nullptr;
  SQLHDBC dbc = nullptr;

  static void SetUpTestSuite() {
  }

  static void TearDownTestSuite() {
  }     

   void SetUp() override {
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    builder = ConnectionStringBuilder();
    builder.withPort(MYSQL_PORT).withLogQuery(true);
   }
    
   void TearDown() override {}
    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
};

TEST_F(SecretsManagerIntegrationTest, EnableSecretsManager) {
  connection_string = builder.withDSN(dsn).withServer(MYSQL_CLUSTER_URL).withAuthMode("SECRETS MANAGER").withAuthRegion("us-east-2").withSecretId(SECRETS_ARN).build();
  EXPECT_TRUE(false) << connection_string;
  SQLCHAR conn_out[4096] = "\0";
  SQLSMALLINT len;
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
