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
 * 2. Redistributions in binary form must rproduce the above copyright notice,
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

#ifndef __TOXICTYPE_H__
#define __TOXICTYPE_H__

#include "toxic.h"
#include "bandwidth.h"
#include "latency.h"
#include "limit_data.h"
#include "reset_peer.h"
#include "slicer.h"
#include "timeout.h"
#include "slow_close.h"

/*
    Create Toxics based on the given type.
*/

class TOXIC_TYPE
{
public:
  static std::map<std::string, TOXIC_TYPES> str_to_toxic_types_map;
  static std::map<TOXIC_TYPES, std::string> toxic_types_to_str_map;
  static TOXIC* create_toxic(TOXIPROXY_HTTP_CLIENT* client, const std::string& path, nlohmann::json json_object);
  static TOXIC_TYPES get_toxic_type(std::string type);

private:
  template <typename T>
  static TOXIC* do_create_toxic(TOXIPROXY_HTTP_CLIENT* client, std::string path, nlohmann::json json_object);
};

#endif
