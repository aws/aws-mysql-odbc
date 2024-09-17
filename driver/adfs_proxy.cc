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

#include "adfs_proxy.h"
#include <regex>
#include "driver.h"

#define SIGN_IN_PAGE_URL "/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=urn:amazon:webservices"

std::unordered_map<std::string, TOKEN_INFO> ADFS_PROXY::token_cache;
std::mutex ADFS_PROXY::token_cache_mutex;

ADFS_PROXY::ADFS_PROXY(DBC* dbc, DataSource* ds) : ADFS_PROXY(dbc, ds, nullptr) {};

ADFS_PROXY::ADFS_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  std::string host{static_cast<const char*>(ds->opt_IDP_ENDPOINT)};
  host += ":" + std::to_string(ds->opt_IDP_PORT);

  const int client_connect_timeout = ds->opt_CLIENT_CONNECT_TIMEOUT;
  const int client_socket_timeout = ds->opt_CLIENT_SOCKET_TIMEOUT;
  const bool enable_ssl = ds->opt_ENABLE_SSL;
  this->saml_util = std::make_shared<ADFS_SAML_UTIL>(host, client_connect_timeout, client_socket_timeout, enable_ssl);
}

void ADFS_PROXY::clear_token_cache() {
  std::unique_lock<std::mutex> lock(token_cache_mutex);
  token_cache.clear();
}

ADFS_SAML_UTIL::ADFS_SAML_UTIL(const std::shared_ptr<SAML_HTTP_CLIENT>& client) { this->http_client = client; }

ADFS_SAML_UTIL::ADFS_SAML_UTIL(std::string host, int connect_timeout, int socket_timeout, bool enable_ssl) {
  this->http_client =
      std::make_shared<SAML_HTTP_CLIENT>("https://" + host, connect_timeout, socket_timeout, enable_ssl);
}

std::string ADFS_SAML_UTIL::get_saml_assertion(DataSource* ds) {
  nlohmann::json res;
  try {
    res = this->http_client->get(std::string(SIGN_IN_PAGE_URL));
  } catch (SAML_HTTP_EXCEPTION& e) {
    const std::string error =
        "Failed to get sign-in page from ADFS: " + e.error_message() + ". Please verify your IDP endpoint.";
    throw SAML_HTTP_EXCEPTION(error);
  }

  const auto body = std::string(res);
  std::smatch m;
  if (!std::regex_search(body, m, ADFS_REGEX::FORM_ACTION_PATTERN)) {
    return std::string();
  }
  std::string form_action = unescape_html_entity(m.str(1));
  const std::string params = get_parameters_from_html(ds, body);
  const std::string content = get_form_action_body(form_action, params);
  if (std::regex_search(content, m, ADFS_REGEX::SAML_RESPONSE_PATTERN)) {
    return m.str(1);
  }
  return std::string();
}

std::string ADFS_SAML_UTIL::unescape_html_entity(const std::string& html) {
  std::string retval("");
  int i = 0;
  int length = html.length();
  while (i < length) {
    char c = html[i];
    if (c != '&') {
      retval.append(1, c);
      i++;
      continue;
    }

    if (html.substr(i, 4) == "&lt;") {
      retval.append(1, '<');
      i += 4;
    } else if (html.substr(i, 4) == "&gt;") {
      retval.append(1, '>');
      i += 4;
    } else if (html.substr(i, 5) == "&amp;") {
      retval.append(1, '&');
      i += 5;
    } else if (html.substr(i, 6) == "&apos;") {
      retval.append(1, '\'');
      i += 6;
    } else if (html.substr(i, 6) == "&quot;") {
      retval.append(1, '"');
      i += 6;
    } else {
      retval.append(1, c);
      ++i;
    }
  }
  return retval;
}

std::vector<std::string> ADFS_SAML_UTIL::get_input_tags_from_html(const std::string& body) {
  std::unordered_set<std::string> hashSet;
  std::vector<std::string> retval;

  std::smatch matches;
  std::regex pattern(ADFS_REGEX::INPUT_TAG_PATTERN);
  std::string source = body;
  while (std::regex_search(source, matches, pattern)) {
    std::string tag = matches.str(0);
    std::string tagName = get_value_by_key(tag, std::string("name"));
    std::transform(tagName.begin(), tagName.end(), tagName.begin(), [](unsigned char c) { return std::tolower(c); });
    if (!tagName.empty() && hashSet.find(tagName) == hashSet.end()) {
      hashSet.insert(tagName);
      retval.push_back(tag);
    }

    source = matches.suffix().str();
  }

  return retval;
}

std::string ADFS_SAML_UTIL::get_value_by_key(const std::string& input, const std::string& key) {
  std::string pattern("(");
  pattern += key;
  pattern += ")\\s*=\\s*\"(.*?)\"";

  std::smatch matches;
  if (std::regex_search(input, matches, std::regex(pattern))) {
    MYLOG_TRACE(init_log_file(), 0, "get_value_by_key");
    return unescape_html_entity(matches.str(2));
  }
  return "";
}

std::string ADFS_SAML_UTIL::get_parameters_from_html(DataSource* ds, const std::string& body) {
  std::map<std::string, std::string> parameters;
  for (auto& inputTag : get_input_tags_from_html(body)) {
    std::string name = get_value_by_key(inputTag, std::string("name"));
    std::string value = get_value_by_key(inputTag, std::string("value"));
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const std::string username = static_cast<const char*>(ds->opt_IDP_USERNAME);
    const std::string password = static_cast<const char*>(ds->opt_IDP_PASSWORD);

    if (nameLower.find("username") != std::string::npos) {
      parameters.insert(std::pair<std::string, std::string>(name, username));
    } else if ((nameLower.find("authmethod") != std::string::npos) && !value.empty()) {
      parameters.insert(std::pair<std::string, std::string>(name, value));
    } else if (nameLower.find("password") != std::string::npos) {
      parameters.insert(std::pair<std::string, std::string>(name, password));
    } else if (!name.empty()) {
      parameters.insert(std::pair<std::string, std::string>(name, value));
    }
  }

  // Convert parameters to a & delimited string, e.g. username=u&password=p
  const std::string delimiter = "&";
  const std::string result =
      std::accumulate(parameters.begin(), parameters.end(), std::string(),
                      [delimiter](const std::string& s, const std::pair<const std::string, std::string>& p) {
                        return s + (s.empty() ? std::string() : delimiter) + p.first + "=" + p.second;
                      });

  return result;
}

std::string ADFS_SAML_UTIL::get_form_action_body(const std::string& url, const std::string& params) {
  nlohmann::json res;
  try {
    res = this->http_client->post(url, params, "application/x-www-form-urlencoded");
  } catch (SAML_HTTP_EXCEPTION& e) {
    const std::string error =
        "Failed to get SAML Assertion from ADFS : " + e.error_message() + ". Please verify your ADFS credentials.";
    throw SAML_HTTP_EXCEPTION(error);
  }
  return res.empty() ? "" : res;
}

#ifdef UNIT_TEST_BUILD
ADFS_PROXY::ADFS_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy, std::shared_ptr<AUTH_UTIL> auth_util,
                       const std::shared_ptr<SAML_HTTP_CLIENT>& client)
    : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  this->auth_util = auth_util;
  this->saml_util = std::make_shared<ADFS_SAML_UTIL>(client);
}
#endif

ADFS_PROXY::~ADFS_PROXY() { this->auth_util.reset(); }

bool ADFS_PROXY::connect(const char* host, const char* user, const char* password, const char* database,
                         unsigned int port, const char* socket, unsigned long flags) {
  auto func = std::bind(&CONNECTION_PROXY::connect, next_proxy, host, user, std::placeholders::_1, database, port,
                        socket, flags);
  const char* region =
      ds->opt_FED_AUTH_REGION ? static_cast<const char*>(ds->opt_FED_AUTH_REGION) : Aws::Region::US_EAST_1;
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

  const char* auth_host = ds->opt_FED_AUTH_HOST ? static_cast<const char*>(ds->opt_FED_AUTH_HOST)
                                                : static_cast<const char*>(ds->opt_SERVER);
  const int auth_port = ds->opt_FED_AUTH_PORT;

  std::string auth_token;
  bool using_cached_token;
  std::tie(auth_token, using_cached_token) = this->auth_util->get_auth_token(
      token_cache, token_cache_mutex, auth_host, region, auth_port, ds->opt_UID, ds->opt_AUTH_EXPIRATION);

  bool connect_result = func(auth_token.c_str());
  if (!connect_result) {
    if (using_cached_token) {
      // Retry func with a fresh token
      std::tie(auth_token, using_cached_token) = this->auth_util->get_auth_token(
          token_cache, token_cache_mutex, auth_host, region, auth_port, ds->opt_UID, ds->opt_AUTH_EXPIRATION, true);
      if (func(auth_token.c_str())) {
        return true;
      }
    }

    if (credentials.IsEmpty()) {
      this->set_custom_error_message(
          "Unable to generate temporary AWS credentials from the SAML assertion. Please ensure the ADFS identity "
          "provider is correctly configured with AWS.");
    }
  }

  return connect_result;
}
