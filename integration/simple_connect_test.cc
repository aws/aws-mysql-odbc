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

#include <cstdlib>
#include <sql.h>
#include <sqlext.h>
#include <gtest/gtest.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

static SQLHENV env;
static SQLHDBC dbc;

class FailoverIntegrationTest : public testing::Test
{
protected:

  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");
  char* server = std::getenv("TEST_SERVER");

  SQLCHAR connIn[4096] = {}, connOut[4096] = {}, sqlstate[6] = {}, message[SQL_MAX_MESSAGE_LENGTH] = {};
  SQLSMALLINT len = 0, length = 0;
  SQLINTEGER native_error = 0;

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
  }
};

TEST_F(FailoverIntegrationTest, SimpleTest)
{
  sprintf(reinterpret_cast<char*>(connIn), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;", dsn, user, pwd, server);

  const SQLRETURN rc = SQLDriverConnect(dbc, nullptr, connIn, SQL_NTS, connOut, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
  SQLRETURN drc = SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &length);

  if (SQL_SUCCEEDED(drc))
    printf("# [%6s] %*sd\n", sqlstate, length, message);

  EXPECT_EQ(SQL_SUCCESS, rc);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
