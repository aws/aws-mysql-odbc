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

#include "driver/adfs_proxy.h"
#include "mock_objects.h"
#include "test_utils.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {
const std::string TEST_HOST{"test_host"};
const std::string TEST_USER{"test_user"};
const std::string TEST_ENDPOINT{"test_endpoint"};
const std::string TEST_IDP_USERNAME{"test_idp_username"};
const std::string TEST_IDP_PASSWORD{"test_idp_password"};
const std::string SIGN_IN_PAGE_URL = "/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=urn:amazon:webservices";
const nlohmann::json TEST_SIGN_IN_PAGE =
    "<form action=\"/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=urn:amazon:webservices\">\n<input id=\"userNameInput\" "
    "name=\"UserName\"/>\n<input id=\"passwordInput\" name=\"Password\"/>\n<input id=\"optionForms\" "
    "name=\"AuthMethod\" "
    "value=\"FormsAuthentication\"/>\n";
const nlohmann::json TEST_SIGN_IN_RESPONSE =
    "name=\"SAMLResponse\" "
    "value=\"PHNhbWwycDpSZXNwb25zZSBEZXN0aW5hdGlvbj0iaHR0cHM6Ly9zaWduaW4uYXdzLmFtYXpvbi5jb20vc2FtbCI+"
    "PC9zYW1sMnA6UmVzcG9uc2U+\"/>";
const nlohmann::json EXPECTED_ASSERTION =
    "PHNhbWwycDpSZXNwb25zZSBEZXN0aW5hdGlvbj0iaHR0cHM6Ly9zaWduaW4uYXdzLmFtYXpvbi5jb20vc2FtbCI+"
    "PC9zYW1sMnA6UmVzcG9uc2U+";
}  // namespace

static SQLHENV env;
static Aws::SDKOptions options;

class AdfsProxyTest : public testing::Test {
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
    ds->opt_UID.set_remove_brackets(to_sqlwchar_string(TEST_USER).c_str(), TEST_USER.size());
    ds->opt_IDP_USERNAME.set_remove_brackets(to_sqlwchar_string(TEST_IDP_USERNAME).c_str(), TEST_IDP_USERNAME.size());
    ds->opt_IDP_PASSWORD.set_remove_brackets(to_sqlwchar_string(TEST_IDP_PASSWORD).c_str(), TEST_IDP_PASSWORD.size());
    ds->opt_IDP_ENDPOINT.set_remove_brackets(to_sqlwchar_string(TEST_ENDPOINT).c_str(), TEST_ENDPOINT.size());

    mock_saml_http_client = std::make_shared<MOCK_SAML_HTTP_CLIENT>(TEST_ENDPOINT, 10, 10, true);
    mock_auth_util = std::make_shared<MOCK_AUTH_UTIL>();
  }

  void TearDown() override { cleanup_odbc_handles(nullptr, dbc, ds); }
};

TEST_F(AdfsProxyTest, GetSAMLAssertion) {
  const httplib::Headers response_body = {{"Set-Cookie", "cookie"}};
  const nlohmann::json expected_cookie = {{"Cookie", "cookie"}};
  const std::string expected_post_body =
      "AuthMethod=FormsAuthentication&Password=test_idp_password&UserName=test_idp_username";
  const httplib::Headers header = {};

  EXPECT_CALL(*mock_saml_http_client, get(StrEq(SIGN_IN_PAGE_URL), header)).WillOnce(Return(TEST_SIGN_IN_PAGE));
  EXPECT_CALL(*mock_saml_http_client,
              post(StrEq(SIGN_IN_PAGE_URL), expected_post_body, "application/x-www-form-urlencoded"))
      .WillOnce(Return(TEST_SIGN_IN_RESPONSE));

  ADFS_SAML_UTIL adfs_util(mock_saml_http_client);

  const std::string assertion = adfs_util.get_saml_assertion(ds);
  EXPECT_EQ(EXPECTED_ASSERTION, assertion);
}
