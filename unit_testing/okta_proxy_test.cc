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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {
const std::string TEST_HOST{"test_host"};
const std::string TEST_REGION{"test_region"};
const std::string TEST_USER{"test_user"};
const std::string TEST_APP_ID{"test_app"};
const std::string TEST_ENDPOINT{"test_endpoint"};
const std::string TEST_IDP_USERNAME{"test_idp_username"};
const std::string TEST_IDP_PASSWORD{"test_idp_password"};

const nlohmann::json TEST_SESSION_TOKEN = {{"sessionToken", "20111sTEtWA8_kJzLH-JQ87ScdVRZOa6NcaX9-letters"}};
const std::string EXPECTED_TOKEN = "20111sTEtWA8_kJzLH-JQ87ScdVRZOa6NcaX9-letters";

const nlohmann::json TEST_ASSERTION =
    "input name=\"SAMLResponse\" type=\"hidden\" "
    "value=\"PHNhbWwycDpSZXNwb25zZSBEZXN0aW5hdGlvbj0iaHR0cHM6Ly9zaWduaW4uYXdzLmFtYXpvbi5jb20vc2FtbCI+"
    "PC9zYW1sMnA6UmVzcG9uc2U+\"/>";
const nlohmann::json EXPECTED_ASSERTION =
    "PHNhbWwycDpSZXNwb25zZSBEZXN0aW5hdGlvbj0iaHR0cHM6Ly9zaWduaW4uYXdzLmFtYXpvbi5jb20vc2FtbCI+PC9zYW1sMnA6UmVzcG9uc2U+";

constexpr unsigned int TEST_PORT = 3306;
constexpr unsigned int TEST_EXPIRATION = 100;
}  // namespace

static SQLHENV env;
static Aws::SDKOptions options;

class OktaProxyTest : public testing::Test {
 protected:
  DBC* dbc;
  DataSource* ds;
  std::shared_ptr<MOCK_AUTH_UTIL> mock_auth_util;
  std::shared_ptr<MOCK_SAML_HTTP_CLIENT> mock_saml_http_client;

  static void SetUpTestSuite() {
    Aws::InitAPI(options);
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
  }

  static void TearDownTestSuite() {
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    Aws::ShutdownAPI(options);
  }

  void SetUp() override {
    SQLHDBC hdbc = nullptr;
    SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
    dbc = static_cast<DBC*>(hdbc);
    ds = new DataSource();

    ds->opt_AUTH_HOST.set_remove_brackets(to_sqlwchar_string(TEST_HOST).c_str(), TEST_HOST.size());
    ds->opt_AUTH_REGION.set_remove_brackets(to_sqlwchar_string(TEST_REGION).c_str(), TEST_REGION.size());
    ds->opt_UID.set_remove_brackets(to_sqlwchar_string(TEST_USER).c_str(), TEST_USER.size());
    ds->opt_IDP_USERNAME.set_remove_brackets(to_sqlwchar_string(TEST_IDP_USERNAME).c_str(), TEST_IDP_USERNAME.size());
    ds->opt_IDP_PASSWORD.set_remove_brackets(to_sqlwchar_string(TEST_IDP_PASSWORD).c_str(), TEST_IDP_PASSWORD.size());
    ds->opt_IDP_ENDPOINT.set_remove_brackets(to_sqlwchar_string(TEST_ENDPOINT).c_str(), TEST_ENDPOINT.size());
    ds->opt_APP_ID.set_remove_brackets(to_sqlwchar_string(TEST_APP_ID).c_str(), TEST_APP_ID.size());
    ds->opt_AUTH_PORT = TEST_PORT;
    ds->opt_AUTH_EXPIRATION = TEST_EXPIRATION;

    mock_saml_http_client = std::make_shared<MOCK_SAML_HTTP_CLIENT>(TEST_ENDPOINT);
    mock_auth_util = std::make_shared<MOCK_AUTH_UTIL>();
  }

  void TearDown() override { cleanup_odbc_handles(nullptr, dbc, ds); }
};

TEST_F(OktaProxyTest, GetSAMLURL) {
  const std::string expected_uri = "/app/amazon_aws/test_app/sso/saml";

  auto okta_util = OKTA_SAML_UTIL(mock_saml_http_client);
  const std::string url = OKTA_SAML_UTIL::get_saml_url(ds);
  EXPECT_EQ(expected_uri, url);
};

TEST_F(OktaProxyTest, GetSessionToken) {
  const nlohmann::json request_body = {{"username", "test_idp_username"}, {"password", "test_idp_password"}};
  EXPECT_CALL(*mock_saml_http_client, post(StrEq("/api/v1/authn"), request_body)).WillOnce(Return(TEST_SESSION_TOKEN));

  OKTA_SAML_UTIL okta_util(mock_saml_http_client);
  const std::string token = okta_util.get_session_token(ds);
  EXPECT_EQ(EXPECTED_TOKEN, token);
};

TEST_F(OktaProxyTest, GetSAMLAssertion) {
  const std::string expected_uri =
      "/app/amazon_aws/test_app/sso/saml?onetimetoken=20111sTEtWA8_kJzLH-JQ87ScdVRZOa6NcaX9-letters";
  const nlohmann::json request_body = {{"username", "test_idp_username"}, {"password", "test_idp_password"}};

  EXPECT_CALL(*mock_saml_http_client, post(StrEq("/api/v1/authn"), request_body)).WillOnce(Return(TEST_SESSION_TOKEN));
  EXPECT_CALL(*mock_saml_http_client, get(_)).WillOnce(Return(TEST_ASSERTION));

  OKTA_SAML_UTIL okta_util(mock_saml_http_client);

  const std::string assertion = okta_util.get_saml_assertion(ds);
  EXPECT_EQ(EXPECTED_ASSERTION, assertion);
}
