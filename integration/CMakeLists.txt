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

project(integration)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 11)

#-------------- unixodbc/iodbc/win -------------------
if(WIN32)
  set(ODBCLIB odbc32)
else(WIN32)
  if(WITH_UNIXODBC)
    set(ODBCLIB odbc)
  else(WITH_UNIXODBC)
    set(ODBCLIB iodbc)
  endif(WITH_UNIXODBC)
endif(WIN32)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)
FetchContent_Declare(
  awssdk
  GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp
)
FetchContent_Declare(
  toxiproxy
  GIT_REPOSITORY https://github.com/Bit-Quill/toxiproxy-cpp
  GIT_TAG main
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Only builds the RDS SDK
set(BUILD_ONLY "rds" CACHE INTERNAL "")
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)

set(CMAKE_EXE_LINKER_FLAGS_DEBUGOPT
  ""
  CACHE STRING "Flags used for linking binaries during coverage builds."
  FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT
  ""
  CACHE STRING "Flags used by the shared libraries linker during coverage builds."
  FORCE)
mark_as_advanced(
  CMAKE_CXX_FLAGS_DEBUGOPT
  CMAKE_C_FLAGS_DEBUGOPT
  CMAKE_EXE_LINKER_FLAGS_DEBUGOPT
  CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT)
set(AWS_SDK_DEPENDENCIES aws-c-auth aws-c-cal aws-c-common aws-c-compression
  aws-c-event-stream aws-checksums aws-c-http aws-c-io aws-c-mqtt
  aws-crt-cpp aws-c-s3 aws-cpp-sdk-core aws-cpp-sdk-rds)

FetchContent_MakeAvailable(googletest toxiproxy awssdk)

enable_testing()

# Ordering matters, test will run in order of top to bottom
add_executable(
  integration
  base_failover_integration_test.cc
  network_failover_integration_test.cc
  failover_integration_test.cc
)

include(GoogleTest)

gtest_discover_tests(integration)

target_include_directories(integration PUBLIC "${httplib_SOURCE_DIR}")

target_link_libraries(
  integration
  nlohmann_json::nlohmann_json
  ${ODBCLIB}
  gtest_main
  toxiproxy
  ${AWS_SDK_DEPENDENCIES}
)

set_target_properties(integration PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
