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

#ifndef __OKTA_PROXY__
#define __OKTA_PROXY__

#include <regex>
#include <unordered_map>
#include "auth_util.h"
#include "saml_http_client.h"
#include "saml_util.h"

namespace {
const std::regex SAML_RESPONSE_PATTERN(R"#(name=\"SAMLResponse\".+value=\"(.+)\"/\>)#", std::regex_constants::icase);
}

class OKTA_SAML_UTIL : public SAML_UTIL {
 public:
  OKTA_SAML_UTIL(const std::shared_ptr<SAML_HTTP_CLIENT>& client);
  OKTA_SAML_UTIL(std::string host, int connect_timeout, int socket_timeout, bool enable_ssl);
  std::string get_saml_assertion(DataSource* ds) override;
  std::string get_session_token(DataSource* ds) const;
  static std::string get_saml_url(DataSource* ds);
  std::shared_ptr<SAML_HTTP_CLIENT> http_client;

 private:
  static std::string replace_all(std::string str, const std::string& from, const std::string& to);
};

class OKTA_PROXY : public CONNECTION_PROXY {
 public:
  OKTA_PROXY() = default;
  OKTA_PROXY(DBC* dbc, DataSource* ds);
  OKTA_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy);
#ifdef UNIT_TEST_BUILD
  OKTA_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy, const std::shared_ptr<AUTH_UTIL>& auth_util,
             const std::shared_ptr<SAML_HTTP_CLIENT>& client);
#endif
  ~OKTA_PROXY() override;

  bool connect(const char* host, const char* user, const char* password, const char* database, unsigned int port,
               const char* socket, unsigned long flags) override;

 protected:
  static std::unordered_map<std::string, TOKEN_INFO> token_cache;
  static std::mutex token_cache_mutex;
  std::shared_ptr<AUTH_UTIL> auth_util;
  std::shared_ptr<OKTA_SAML_UTIL> saml_util;
  bool using_cached_token = false;
  static void clear_token_cache();
  bool invoke_func_with_fed_credentials(std::function<bool(const char*)> func);

#ifdef UNIT_TEST_BUILD
  // Allows for testing private/protected methods
  friend class TEST_UTILS;
#endif
};

#endif