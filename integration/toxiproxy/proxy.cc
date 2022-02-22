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

#include "proxy.h"

#include <iostream>
#include <utility>

PROXY::PROXY(TOXIPROXY_HTTP_CLIENT* client, const std::string& path, nlohmann::json json_object)
  : client(client), path(path)
{
  set_from_json(std::move(json_object));
}

std::string PROXY::get_name()
{
  return name;
}

std::string PROXY::get_listen()
{
  return listen;
}

std::string PROXY::get_upstream()
{
  return upstream;
}

bool PROXY::is_enabled() const
{
  return enabled;
}

TOXIC_LIST* PROXY::get_toxics() const
{
  return toxic_list;
}

void PROXY::set_listen(std::string new_listen)
{
  set_from_json(client->post(path, "listen", std::move(new_listen)));
}

void PROXY::set_upstream(std::string new_upstream)
{
  set_from_json(client->post(path, "upstream", std::move(new_upstream)));
}

void PROXY::enable()
{
  set_from_json(client->post(path, "enabled", true));
}

void PROXY::disable()
{
  set_from_json(client->post(path, "enabled", false));
}

void PROXY::delete_path() const
{
  client->delete_path(path);
}

void PROXY::reset()
{
  set_from_json(client->get(path));
}

void PROXY::set_from_json(nlohmann::json json_object)
{
  name = json_object["name"].get<std::string>();
  listen = json_object["listen"].get<std::string>();
  upstream = json_object["upstream"].get<std::string>();
  enabled = json_object["enabled"].get<bool>();

  toxic_list = new TOXIC_LIST(client, path + "/toxics");
}
