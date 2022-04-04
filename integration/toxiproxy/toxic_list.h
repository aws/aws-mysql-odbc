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

#ifndef __TOXICLIST_H__
#define __TOXICLIST_H__

#include <type_traits>
#include <utility>

#include "toxic_type.h"

class TOXIC_LIST
{
public:
  TOXIC_LIST(TOXIPROXY_HTTP_CLIENT* client, std::string path) : client{ client }, path(std::move(path)) {}
  TOXIC* get(std::string& name);

  template <typename T, typename std::enable_if<std::is_base_of<TOXIC, T>::value>::type* = nullptr>
  std::vector<T> get_all();

  template <typename T, typename std::enable_if<std::is_base_of<TOXIC, T>::value>::type* = nullptr>
  T get(std::string name, T type);

  BANDWIDTH* bandwidth(const std::string& name, TOXIC_DIRECTION direction, long rate) const;
  LATENCY* latency(const std::string& name, TOXIC_DIRECTION direction, long latency) const;
  SLICER* slicer(const std::string& name, TOXIC_DIRECTION direction, long delay) const;
  SLOW_CLOSE* slow_close(const std::string& name, TOXIC_DIRECTION direction, long delay) const;
  TIMEOUT* timeout(const std::string& name, TOXIC_DIRECTION direction, long timeout) const;
  LIMIT_DATA* limit_data(const std::string& name, TOXIC_DIRECTION direction, long bytes) const;
  RESET_PEER* reset_peer(const std::string& name, TOXIC_DIRECTION direction, long timeout) const;

private:
  TOXIPROXY_HTTP_CLIENT* client;
  std::string path;
  std::string get_toxic_path(const std::string& name) const;
};

#endif
