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
#include <sql.h>
#include <sqlext.h>

#include "connection_string_builder.h"
#include "integration_test_utils.h"

// Connection string parameters
static char* test_dsn;
static char* test_db;
static char* test_user;
static char* test_pwd;
static unsigned int test_port;
static char* iam_user;
static char* test_region;

static std::string test_endpoint;

class IamAuthenticationIntegrationTest : public testing::Test {
protected:
    std::string default_connection_string = "";
    SQLHENV env = nullptr;
    SQLHDBC dbc = nullptr;

    static void SetUpTestSuite() {
        test_endpoint = std::getenv("TEST_SERVER");

        test_dsn = std::getenv("TEST_DSN");
        test_db = std::getenv("TEST_DATABASE");
        test_user = std::getenv("TEST_UID");
        test_pwd = std::getenv("TEST_PASSWORD");
        test_port = INTEGRATION_TEST_UTILS::str_to_int(
            INTEGRATION_TEST_UTILS::get_env_var("MYSQL_PORT", "3306"));
        iam_user = INTEGRATION_TEST_UTILS::get_env_var("IAM_USER", "john_doe");
        test_region = INTEGRATION_TEST_UTILS::get_env_var("RDS_REGION", "us-east-2");

        auto conn_str = ConnectionStringBuilder(test_dsn, test_endpoint, test_port)
                            .withUID(test_user)
                            .withPWD(test_pwd)
                            .withDatabase(test_db)
                            .getString();

        SQLHENV env1 = nullptr;
        SQLHDBC dbc1 = nullptr;
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env1);
        SQLSetEnvAttr(env1, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        SQLAllocHandle(SQL_HANDLE_DBC, env1, &dbc1);

        SQLCHAR conn_out[4096] = "\0";
        SQLSMALLINT len;
        EXPECT_EQ(SQL_SUCCESS, 
            SQLDriverConnect(dbc1, nullptr, AS_SQLCHAR(conn_str.c_str()), SQL_NTS,
                conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

        SQLHSTMT stmt = nullptr;
        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc1, &stmt));

        char query_buffer[200];
        sprintf(query_buffer, "DROP USER IF EXISTS %s;", iam_user);
        SQLExecDirect(stmt, AS_SQLCHAR(query_buffer), SQL_NTS);

        memset(query_buffer, 0, sizeof(query_buffer));
        sprintf(query_buffer, "CREATE USER %s IDENTIFIED WITH AWSAuthenticationPlugin AS 'RDS';", iam_user);
        EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(stmt, AS_SQLCHAR(query_buffer), SQL_NTS));

        memset(query_buffer, 0, sizeof(query_buffer));
        sprintf(query_buffer, "GRANT ALL ON `%`.* TO %s@`%`;", iam_user);
        EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(stmt, AS_SQLCHAR(query_buffer), SQL_NTS));

        EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, stmt));
        EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc1));
        
        if (nullptr != stmt) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        }
        if (nullptr != dbc1) {
            SQLDisconnect(dbc1);
            SQLFreeHandle(SQL_HANDLE_DBC, dbc1);
        }
        if (nullptr != env1) {
            SQLFreeHandle(SQL_HANDLE_ENV, env1);
        }
    }

    static void TearDownTestSuite() {
    }

    void SetUp() override {
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

        default_connection_string = ConnectionStringBuilder(test_dsn, test_endpoint, test_port)
                                        .withEnableClusterFailover(false)  // Failover interferes with some of our tests
                                        .withAuthMode("IAM")
                                        .withAuthRegion(test_region)
                                        .withAuthExpiration(900)
                                        .getString();
    }

    void TearDown() override {
        if (nullptr != dbc) {
            SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        }
        if (nullptr != env) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
    }
};

// Tests a simple IAM connection with all expected fields provided.
TEST_F(IamAuthenticationIntegrationTest, SimpleIamConnection) {
    auto connection_string = ConnectionStringBuilder(default_connection_string)
                                 .withAuthHost(test_endpoint)
                                 .withUID(iam_user)
                                 .withAuthPort(test_port)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);
    
    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect to the provided server
// when the Auth host is not provided.
TEST_F(IamAuthenticationIntegrationTest, ServerWithNoAuthHost) {
    auto connection_string = ConnectionStringBuilder(default_connection_string)
                                 .withAuthHost(test_endpoint)
                                 .withUID(iam_user)
                                 .withAuthPort(test_port)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect via the provided port
// when the auth port is not provided.
TEST_F(IamAuthenticationIntegrationTest, PortWithNoAuthPort) {
    auto connection_string = ConnectionStringBuilder(default_connection_string)
                                 .withAuthHost(test_endpoint)
                                 .withUID(iam_user)
                                 .withAuthPort(test_port)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect
// when given an IP address instead of a cluster name.
TEST_F(IamAuthenticationIntegrationTest, ConnectToIpAddress) {
    auto ip_address = INTEGRATION_TEST_UTILS::host_to_IP(test_endpoint);
    
    auto connection_string = ConnectionStringBuilder(test_dsn, ip_address, test_port)
                                 .withEnableClusterFailover(false)  // Failover interferes with some of our tests
                                 .withAuthMode("IAM")
                                 .withAuthRegion(test_region)
                                 .withAuthExpiration(900)
                                 .withAuthHost(test_endpoint)
                                 .withUID(iam_user)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);
    
    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that IAM connection will still connect
// when given a wrong password (because the password gets replaced by the auth token).
TEST_F(IamAuthenticationIntegrationTest, WrongPassword) {
    auto connection_string = ConnectionStringBuilder(default_connection_string)
                                 .withAuthHost(test_endpoint)
                                 .withUID(iam_user)
                                 .withPWD("WRONG_PASSWORD")
                                 .withAuthPort(test_port)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, rc);

    rc = SQLDisconnect(dbc);
    EXPECT_EQ(SQL_SUCCESS, rc);
}

// Tests that the IAM connection will fail when provided a wrong user.
TEST_F(IamAuthenticationIntegrationTest, WrongUser) {
    auto connection_string = ConnectionStringBuilder(default_connection_string)
                                 .withAuthHost(test_endpoint)
                                 .withUID("WRONG_USER")
                                 .withAuthPort(test_port)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH] = "\0", state[6] = "\0";
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err,
        msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);
    
    const std::string state_str = reinterpret_cast<char*>(state);
    EXPECT_EQ("HY000", state_str);
}

// Tests that the IAM connection will fail when provided an empty user.
TEST_F(IamAuthenticationIntegrationTest, EmptyUser) {
    auto connection_string = ConnectionStringBuilder(default_connection_string)
                                 .withAuthHost(test_endpoint)
                                 .withUID("")
                                 .withAuthPort(test_port)
                                 .getString();

    SQLCHAR conn_out[4096] = "\0";
    SQLSMALLINT len;
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS,
        conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, rc);

    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH] = "\0", state[6] = "\0";
    rc = SQLError(nullptr, dbc, nullptr, state, &native_err,
        msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    EXPECT_EQ(SQL_SUCCESS, rc);

    const std::string state_str = reinterpret_cast<char*>(state);
    EXPECT_EQ("HY000", state_str);
}
