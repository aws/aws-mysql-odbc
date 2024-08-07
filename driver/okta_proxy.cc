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

#include <functional>

#include "driver.h"
#include "okta_proxy.h"
#include "saml_http_client.h"

#define OKTA_AWS_APP_NAME "amazon_aws"

std::unordered_map<std::string, TOKEN_INFO> OKTA_PROXY::token_cache;
std::mutex OKTA_PROXY::token_cache_mutex;

OKTA_PROXY::OKTA_PROXY(DBC* dbc, DataSource* ds) : OKTA_PROXY(dbc, ds, nullptr) {};

OKTA_PROXY::OKTA_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  const std::string idp_host{static_cast<const char*>(ds->opt_IDP_ENDPOINT)};
  this->saml_util = std::make_shared<OKTA_SAML_UTIL>(idp_host);
}

bool OKTA_PROXY::connect(const char* host, const char* user, const char* password, const char* database,
                         unsigned int port, const char* socket, unsigned long flags) {
  auto f = std::bind(&CONNECTION_PROXY::connect, next_proxy, host, user, std::placeholders::_1, database, port, socket,
                     flags);
  return invoke_func_with_fed_credentials(f);
}

bool OKTA_PROXY::invoke_func_with_fed_credentials(std::function<bool(const char*)> func) {
  const char* region = ds->opt_AUTH_REGION ? static_cast<const char*>(ds->opt_AUTH_REGION) : Aws::Region::US_EAST_1;
  std::string assertion;
  try {
    assertion = this->saml_util->get_saml_assertion(ds);
  } catch (SAML_HTTP_EXCEPTION& e) {
    this->set_custom_error_message(e.error_message().c_str());
    return false;
  }

  auto idp_host = static_cast<const char*>(ds->opt_IDP_ENDPOINT);
  auto iam_role_arn = static_cast<const char*>(ds->opt_IAM_ROLE_ARN);
  auto idp_arn = static_cast<const char*>(ds->opt_IAM_IDP_ARN);
  const Aws::Auth::AWSCredentials credentials =
      this->saml_util->get_aws_credentials(idp_host, region, iam_role_arn, idp_arn, assertion);
  this->auth_util = std::make_shared<AUTH_UTIL>(region, credentials);

  const char* AUTH_HOST =
      ds->opt_AUTH_HOST ? static_cast<const char*>(ds->opt_AUTH_HOST) : static_cast<const char*>(ds->opt_SERVER);
  int auth_port = ds->opt_AUTH_PORT;
  if (auth_port == UNDEFINED_PORT) {
    // Use regular port if user does not provide an alternative port for AWS authentication
    auth_port = ds->opt_PORT;
  }

  std::string auth_token = this->auth_util->get_auth_token(AUTH_HOST, region, auth_port, ds->opt_UID);

  bool connect_result = func(auth_token.c_str());
  if (!connect_result) {
    if (using_cached_token) {
      // Retry func with a fresh token
      auth_token = this->auth_util->get_auth_token(AUTH_HOST, region, auth_port, ds->opt_UID);
      if (func(auth_token.c_str())) {
        return true;
      }
    }

    if (credentials.IsEmpty()) {
      this->set_custom_error_message(
          "Unable to generate temporary AWS credentials from the SAML assertion. Please ensure the Okta identity "
          "provider is correctly configured with AWS.");
    }
  }

  return connect_result;
}

OKTA_PROXY::~OKTA_PROXY() {
  this->auth_util.reset();
  this->saml_util.reset();
}

#ifdef UNIT_TEST_BUILD
OKTA_PROXY::OKTA_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy,
                       const std::shared_ptr<AUTH_UTIL>& auth_util, const std::shared_ptr<SAML_HTTP_CLIENT>& client)
    : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  this->auth_util = auth_util;
  this->saml_util = std::make_shared<OKTA_SAML_UTIL>(client);
}
#endif

void OKTA_PROXY::clear_token_cache() {
  std::unique_lock<std::mutex> lock(token_cache_mutex);
  token_cache.clear();
}

OKTA_SAML_UTIL::OKTA_SAML_UTIL(const std::shared_ptr<SAML_HTTP_CLIENT>& client) { this->http_client = client; }

OKTA_SAML_UTIL::OKTA_SAML_UTIL(std::string host) {
  this->http_client = std::make_shared<SAML_HTTP_CLIENT>("https://" + host);
}

std::string OKTA_SAML_UTIL::get_saml_url(DataSource* ds) {
  auto app_id = static_cast<const char*>(ds->opt_APP_ID);

  return "/app/" + std::string(OKTA_AWS_APP_NAME) + "/" + app_id + "/sso/saml";
}

std::string OKTA_SAML_UTIL::get_session_token(DataSource* ds) const {
  const std::string username = static_cast<const char*>(ds->opt_IDP_USERNAME);
  const std::string password = static_cast<const char*>(ds->opt_IDP_PASSWORD);

  const std::string session_token_endpoint = "/api/v1/authn";
  const nlohmann::json request_body = {{"username", username}, {"password", password}};
  nlohmann::json res;
  try {
    res = this->http_client->post(session_token_endpoint, request_body);
  } catch (SAML_HTTP_EXCEPTION& e) {
    const std::string error =
        "Failed to get session token from Okta : " + e.error_message() + ". Please verify your Okta credentials.";
    throw SAML_HTTP_EXCEPTION(error);
  }
  if (res.empty()) {
    return "";
  }
  return res["sessionToken"];
}

std::string OKTA_SAML_UTIL::get_saml_assertion(DataSource* ds) {
  const std::string token = this->get_session_token(ds);
  nlohmann::json res;
  try {
    res = this->http_client->get(this->get_saml_url(ds) + "?onetimetoken=" + token);
  } catch (SAML_HTTP_EXCEPTION& e) {
    const std::string error =
        "Failed to get SAML assertion from Okta : " + e.error_message() + ". Please verify your Okta identity provider configuration on AWS.";
    throw SAML_HTTP_EXCEPTION(error);
  }
  const auto body = std::string(res);
  auto f = [body](const std::regex& pattern) {
    if (std::smatch m; std::regex_search(body, m, pattern)) {
      std::string saml = m.str(1);

      saml = replace_all(saml, "&#x2b;", "+");
      saml = replace_all(saml, "&#x3d;", "=");
      return saml;
    }
    return std::string();
  };

  return f(SAML_RESPONSE_PATTERN);
}

std::string OKTA_SAML_UTIL::replace_all(std::string str, const std::string& from, const std::string& to) {
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str = str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}
