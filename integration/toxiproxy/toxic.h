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

#ifndef __TOXIC_H__
#define __TOXIC_H__

#include <ctype.h>
#include "toxiproxy_http_client.h"

/*
    Enums of all available types of Toxics.
*/

enum class TOXIC_TYPES
{
  BANDWIDTH,
  LIMIT_DATA,
  LATENCY,
  SLICER,
  SLOW_CLOSE,
  RESET_PEER,
  TIMEOUT,
  INVALID
};

/*
    Base class for all types of Toxics. Toxics maniuplate the pipe between the client and the upstream, 
    and are added to or removed from proxies via the TOXIPROXY_HTTP_CLIENT.
*/

class TOXIC
{
public:
  TOXIC(TOXIPROXY_HTTP_CLIENT* client, const std::string& toxic_list_path, const std::string& name, TOXIC_DIRECTION stream);
  TOXIC(TOXIPROXY_HTTP_CLIENT* client, std::string path, nlohmann::json json_object);
  virtual ~TOXIC() = default;
  void remove() const;

  virtual void set_attributes(nlohmann::json attributes) = 0;
  virtual nlohmann::json get_attributes() = 0;
  virtual TOXIC_TYPES get_type() = 0;

  void create_toxic(const std::string& toxic_list_path);
  void post_attribute(std::string name, long value);
  void set_from_json(nlohmann::json json_object);
  void set_toxicity(double toxicity_val);
  std::string get_name() const;
  TOXIC_DIRECTION get_stream() const;
  double get_toxicity() const;

private:
  TOXIPROXY_HTTP_CLIENT* client;
  std::string path;
  std::string name;
  TOXIC_DIRECTION stream;
  double toxicity;
};

#endif
