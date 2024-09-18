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

#include "saml_http_client.h"
#include <utility>

#include "mylog.h"

#define MAX_REDIRECT_COUNT 20

#if !defined(WIN32)
#define stricmp strcasecmp
#endif

SAML_HTTP_CLIENT::SAML_HTTP_CLIENT(std::string host, int connect_timeout, int socket_timeout, bool enable_ssl)
    : host{std::move(host)}, connect_timeout(connect_timeout), socket_timeout(socket_timeout), enable_ssl(enable_ssl) {}

httplib::Client SAML_HTTP_CLIENT::get_client() const {
  httplib::Client client(host);
  client.enable_server_certificate_verification(enable_ssl);
  client.set_connection_timeout(connect_timeout);
  client.set_read_timeout(socket_timeout);
  client.set_write_timeout(socket_timeout);
  return client;
}


nlohmann::json SAML_HTTP_CLIENT::post(const std::string& path, const std::string& value,
                                      const std::string& content_type) {
  httplib::Client client = this->get_client();
  auto res = client.Post(path.c_str(), value, content_type);
  if (!res) {
    throw SAML_HTTP_EXCEPTION("Post request failed");
  }
  if (res->status == httplib::StatusCode::OK_200) {
    if (stricmp(content_type.c_str(), "application/json") == 0) {
      return nlohmann::json::parse(res->body);
    }
    return res->body;
  }

  int count = MAX_REDIRECT_COUNT;
  while (res->status == httplib::StatusCode::Found_302 && count > 0) {
    auto headers = res->headers;
    auto pos = headers.find("location");
    if (pos != headers.end()) {
      httplib::Headers cookies = {};
      std::string cookiestr;
      for (auto const& x : headers) {
        if (stricmp(x.first.c_str(), "Set-Cookie") == 0) {
          cookiestr += x.second;
          cookiestr += ";";
        }
      }
      cookies.emplace("Cookie", cookiestr);

      httplib::Client redirect_client = this->get_client();
      res = redirect_client.Get(pos->second.c_str(), cookies);
      count--;
    }

    if (res->status == httplib::StatusCode::OK_200) {
      if (stricmp(content_type.c_str(), "application/json") == 0) {
        return nlohmann::json::parse(res->body);
      }

      return res->body;
    }
  }
  throw SAML_HTTP_EXCEPTION(std::to_string(res->status) + " " + res->reason);
}

nlohmann::json SAML_HTTP_CLIENT::get(const std::string& path, const httplib::Headers& headers) {
  httplib::Client client = this->get_client();
  client.set_follow_location(true);

  if (auto res = (headers.empty() ? client.Get(path) : client.Get(path, headers))) {
    if (res->status == httplib::StatusCode::OK_200) {
      return res->body;
    }
    throw SAML_HTTP_EXCEPTION(std::to_string(res->status) + " " + res->reason);
  }
  throw SAML_HTTP_EXCEPTION("Get request failed");
}
