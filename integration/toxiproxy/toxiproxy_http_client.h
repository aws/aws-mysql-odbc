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

#ifndef __TOXIPROXYHTTPCLIENT_H__
#define __TOXIPROXYHTTPCLIENT_H__

#include <nlohmann/json.hpp>
#include <httplib.h>

/*
  Enums representing the stream direction.
  Upstream = client -> server.
  Downstream = server -> client.
*/

enum class TOXIC_DIRECTION
{
  UPSTREAM = 0,
  DOWNSTREAM = 1
};

/*
    HTTP Client to communicate to the toxiproxy daemon.
*/

class TOXIPROXY_HTTP_CLIENT
{
public:
  static std::map<std::string, TOXIC_DIRECTION> str_to_toxic_direction_map;
  static std::map<TOXIC_DIRECTION, std::string> toxic_direction_to_str_map;

  TOXIPROXY_HTTP_CLIENT(std::string protocol, std::string host, int port);
  ~TOXIPROXY_HTTP_CLIENT() = default;

  nlohmann::json post(const std::string& path, std::string name, bool value) const;
  nlohmann::json post(const std::string& path, std::string name, std::string value) const;
  nlohmann::json post(const std::string& path, std::string name, double value) const;
  nlohmann::json post(const std::string& path, nlohmann::json value) const;
  nlohmann::json post(const std::string& path) const;
  nlohmann::json get(const std::string& path) const;
  int delete_path(const std::string& path) const;

private:
  const std::string protocol;
  const std::string host;
  const int port;

  const std::string PROXIES_API = "/proxies/";
  const std::string RESET_API = "/reset";
};

#endif
