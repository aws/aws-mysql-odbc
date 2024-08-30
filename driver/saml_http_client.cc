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

SAML_HTTP_CLIENT::SAML_HTTP_CLIENT(std::string host) : host{std::move(host)} {}

nlohmann::json SAML_HTTP_CLIENT::post(const std::string& path, const nlohmann::json& value) {
  httplib::Client client(host);
  if (auto res = client.Post(path.c_str(), value.dump(), "application/json")) {
    if (res->status == httplib::StatusCode::OK_200) {
      nlohmann::json json_object = nlohmann::json::parse(res->body);
      return json_object;
    }

    throw SAML_HTTP_EXCEPTION(std::to_string(res->status) + " " + res->reason);
  }
  throw SAML_HTTP_EXCEPTION("Post request failed");
}

nlohmann::json SAML_HTTP_CLIENT::get(const std::string& path) {
  httplib::Client client(host);
  client.set_follow_location(true);
  if (auto res = client.Get(path.c_str())) {
    if (res->status == httplib::StatusCode::OK_200) {
      return res->body;
    }
    throw SAML_HTTP_EXCEPTION(std::to_string(res->status) + " " + res->reason);
  }

  throw SAML_HTTP_EXCEPTION("Get request failed");
}
