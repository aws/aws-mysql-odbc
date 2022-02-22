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

#include "latency.h"

void LATENCY::set_attributes(nlohmann::json attributes)
{
  this->latency = attributes["latency"].get<long>();
  this->jitter = attributes["jitter"].get<long>();
}

nlohmann::json LATENCY::get_attributes()
{
  nlohmann::json attribute = {
      {"latency", latency},
      {"jitter", jitter} };
  return attribute;
}

TOXIC_TYPES LATENCY::get_type()
{
  return TOXIC_TYPES::LATENCY;
}

long LATENCY::get_latency() const
{
  return latency;
}

long LATENCY::get_jitter() const
{
  return jitter;
}

LATENCY* LATENCY::set_latency(const long new_latency)
{
  post_attribute("latency", new_latency);
  return this;
}

LATENCY* LATENCY::set_jitter(const long new_jitter)
{
  post_attribute("jitter", new_jitter);
  return this;
}
