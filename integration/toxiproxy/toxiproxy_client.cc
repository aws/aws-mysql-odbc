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

#include "toxiproxy_client.h"

#include <utility>

TOXIPROXY_CLIENT::TOXIPROXY_CLIENT() : TOXIPROXY_CLIENT("localhost", 8474) {}

TOXIPROXY_CLIENT::TOXIPROXY_CLIENT(std::string host, int port) : TOXIPROXY_CLIENT("http", std::move(host), port) {}

TOXIPROXY_CLIENT::TOXIPROXY_CLIENT(std::string protocol, std::string host, int port)
{
  client = new TOXIPROXY_HTTP_CLIENT(std::move(protocol), std::move(host), port);
}

std::vector<PROXY*> TOXIPROXY_CLIENT::get_proxies() const
{
  std::vector<PROXY*> proxies;
  nlohmann::json json_object = client->get(PROXIES_API);
  if (!json_object)
    return proxies;

  for (auto elem : json_object.items())
    proxies.push_back(new PROXY(client, PROXIES_API + elem.key(), elem.value()));

  return proxies;
}

PROXY* TOXIPROXY_CLIENT::create_proxy(std::string name, std::string listen, std::string upstream)
{
  const nlohmann::json json_object = {
    {"name", name},
    {"listen", listen},
    {"upstream", upstream}
  };

  return new_proxy_instance(name, client->post(PROXIES_API, json_object));
}

PROXY* TOXIPROXY_CLIENT::get_proxy(const std::string& name)
{
  return new_proxy_instance(name, client->get(PROXIES_API + name));
}

void TOXIPROXY_CLIENT::reset() const
{
  client->post(RESET_API);
  for (const auto elem : proxies)
  {
    elem->reset();
  }
}

PROXY* TOXIPROXY_CLIENT::new_proxy_instance(const std::string& name, nlohmann::json json_object) {
  const auto proxy = new PROXY(client, PROXIES_API + name, std::move(json_object));
  proxies.insert(proxy);
  return proxy;
}
