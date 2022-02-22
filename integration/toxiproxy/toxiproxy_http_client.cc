/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "toxiproxy_http_client.h"

#include <utility>

std::map<std::string, TOXIC_DIRECTION> TOXIPROXY_HTTP_CLIENT::str_to_toxic_direction_map = { {"UPSTREAM", TOXIC_DIRECTION::UPSTREAM}, {"DOWNSTREAM", TOXIC_DIRECTION::DOWNSTREAM} };
std::map<TOXIC_DIRECTION, std::string> TOXIPROXY_HTTP_CLIENT::toxic_direction_to_str_map = { {TOXIC_DIRECTION::UPSTREAM, "upstream"}, {TOXIC_DIRECTION::DOWNSTREAM, "downstream"} };

TOXIPROXY_HTTP_CLIENT::TOXIPROXY_HTTP_CLIENT(std::string protocol, std::string host, int port)
  : protocol(std::move(protocol)), host(std::move(host)), port(port)
{
  // TODO: handle https protocol
}

nlohmann::json TOXIPROXY_HTTP_CLIENT::post(const std::string& path, std::string name, bool value) const
{
  std::string str_value = value ? "true" : "false";
  const httplib::Params param{
    {name, str_value}
  };
  return post(path, param);
}

nlohmann::json TOXIPROXY_HTTP_CLIENT::post(const std::string& path, std::string name, std::string value) const
{
  const httplib::Params param {
  {name, value}
  };
  return post(path, param);
}

nlohmann::json TOXIPROXY_HTTP_CLIENT::post(const std::string& path, std::string name, double value) const
{
  const httplib::Params param {
    {name, std::to_string(value)}
  };
  return post(path, param);
}

nlohmann::json TOXIPROXY_HTTP_CLIENT::post(const std::string& path, nlohmann::json value) const
{
  if (auto res = httplib::Client(host, port).Post(path.c_str(), value.dump(), "application/json"))
  {
    nlohmann::json json_object = nlohmann::json::parse(res->body);
    return json_object;
  }

  return "";
}

nlohmann::json TOXIPROXY_HTTP_CLIENT::post(const std::string& path) const
{
  if (path.empty())
    return -1;
  
  if (auto res = httplib::Client(host, port).Post(path.c_str()))
  {
    nlohmann::json json_object = nlohmann::json::parse(res->body);
    return json_object;
  }

  return "";
}

nlohmann::json TOXIPROXY_HTTP_CLIENT::get(const std::string& path) const
{
  if (path.empty())
    return -1;

  if (auto res = httplib::Client(host, port).Get(path.c_str())) {
    return nlohmann::json::parse(res->body);
  }

  return "";
}

int TOXIPROXY_HTTP_CLIENT::delete_path(const std::string& path) const
{
  if (path.empty())
    return -1;

  if (auto res = httplib::Client(host, port).Delete(path.c_str()))
    return res->status;

  return -1;
}
