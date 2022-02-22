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

#include "toxic_type.h"
#include "toxic_list.h"

#include <utility>

TOXIC* TOXIC_LIST::get(const std::string& name)
{
  const std::string toxic_path = get_toxic_path(std::move(name));
  const nlohmann::json json_object = client->get(toxic_path);

  if (json_object.contains("error")) {
    return nullptr;
  }

  return TOXIC_TYPE::create_toxic(client, toxic_path, json_object);
}

template<typename T, typename std::enable_if<std::is_base_of<TOXIC, T>::value>::type*>
std::vector<T> TOXIC_LIST::get_all()
{
  std::vector<TOXIC*> toxics;

  for (auto json_object : client->get(path))
  {
    auto toxic_name = json_object["name"].get<std::string>();
    TOXIC* toxic = TOXIC_TYPE::create_toxic(client, get_toxic_path(toxic_name), json_object);
    toxics.push_back(toxic);
  }

  return toxics;
}

template<typename T, typename std::enable_if<std::is_base_of<TOXIC, T>::value>::type*>
T TOXIC_LIST::get(std::string name, T type)
{
  TOXIC* toxic = get(name);
  return static_cast<T>(toxic);
}

BANDWIDTH* TOXIC_LIST::bandwidth(const std::string& name, TOXIC_DIRECTION direction, long rate) const
{
  return new BANDWIDTH(client, path, name, direction, rate);
}

LATENCY* TOXIC_LIST::latency(const std::string& name, TOXIC_DIRECTION direction, long latency) const
{
  return new LATENCY(client, path, name, direction, latency);
}

SLICER* TOXIC_LIST::slicer(const std::string& name, TOXIC_DIRECTION direction, long average_size, long delay) const
{
  return new SLICER(client, path, name, direction, delay);
}

SLOW_CLOSE* TOXIC_LIST::slow_close(const std::string& name, TOXIC_DIRECTION direction, long delay) const
{
  return new SLOW_CLOSE(client, path, name, direction, delay);
}

TIMEOUT* TOXIC_LIST::timeout(const std::string& name, TOXIC_DIRECTION direction, long timeout) const
{
  return new TIMEOUT(client, path, name, direction, timeout);
}

LIMIT_DATA* TOXIC_LIST::limit_data(const std::string& name, TOXIC_DIRECTION direction, long bytes) const
{
  return new LIMIT_DATA(client, path, name, direction, bytes);
}

RESET_PEER* TOXIC_LIST::reset_peer(const std::string& name, TOXIC_DIRECTION direction, long timeout) const
{
  return new RESET_PEER(client, path, name, direction, timeout);
}

std::string TOXIC_LIST::get_toxic_path(const std::string& name) const
{
  return path + "/" + name;
}
