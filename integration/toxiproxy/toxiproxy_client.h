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

#ifndef __TOXIPROXY_CLIENT_H__
#define __TOXIPROXY_CLIENT_H__

#include "proxy.h"
#include "toxiproxy_http_client.h"

/*
    Create a PROXY object or retrieves existing proxies.
*/

class TOXIPROXY_CLIENT
{
public:
  TOXIPROXY_CLIENT();
  TOXIPROXY_CLIENT(std::string host, int port);
  TOXIPROXY_CLIENT(std::string protocol, std::string host, int port);
  ~TOXIPROXY_CLIENT() = default;

  std::vector<PROXY*> get_proxies() const;
  PROXY* create_proxy(std::string name, std::string listen, std::string upstream);
  PROXY* get_proxy(const std::string& name);
  void reset() const;

private:
  PROXY* new_proxy_instance(const std::string& name, nlohmann::json json_object);
  TOXIPROXY_HTTP_CLIENT* client;
  const std::string PROXIES_API = "/proxies/";
  const std::string RESET_API = "/reset";

  std::set<PROXY*> proxies;
};

#endif
