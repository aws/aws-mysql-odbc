# 
# AWS ODBC Driver for MySQL
# Copyright Amazon.com Inc. or affiliates.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 

cmake_minimum_required(VERSION 3.14)

project(toxiproxy)

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz
)

FetchContent_Declare(
  httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.10.2.zip
)

set(TOXIPROXY_SRCS toxiproxy_client.cc toxiproxy_http_client.cc toxic.cc 
toxic_type.cc toxic_list.cc timeout.cc slow_close.cc slicer.cc
reset_peer.cc proxy.cc limit_data.cc latency.cc bandwidth.cc)
set(TOXIPROXY_HEADERS toxiproxy_client.h toxiproxy_http_client.h toxic.h 
toxic_type.h toxic_list.h timeout.h slow_close.h slicer.h
reset_peer.h proxy.h limit_data.h latency.h bandwidth.h)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

if(WIN32)
  set(TOXIPROXY_SRCS ${TOXIPROXY_SRCS} ${TOXIPROXY_HEADERS})
endif(WIN32)

FetchContent_MakeAvailable(httplib json)

add_library(toxiproxy ${TOXIPROXY_SRCS} ${TOXIPROXY_HEADERS})

target_include_directories(toxiproxy PUBLIC "${httplib_SOURCE_DIR}")

target_link_libraries(
  toxiproxy
  nlohmann_json::nlohmann_json
)
