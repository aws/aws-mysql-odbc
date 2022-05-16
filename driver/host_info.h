/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __HOSTINFO_H__
#define __HOSTINFO_H__

#include <memory>
#include <string>

enum HOST_STATE { UP, DOWN };

// TODO Think about char types. Using strings for now, but should SQLCHAR *, or CHAR * be employed?
// Most of the strings are for internal failover things
class HOST_INFO {
public:
    HOST_INFO();
    //TODO - probably choose one of the following constructors, or more precisely choose which data type they should take
    HOST_INFO(std::string host, int port);
    HOST_INFO(const char* host, int port);
    HOST_INFO(std::string host, int port, HOST_STATE state, bool is_writer);
    HOST_INFO(const char* host, int port, HOST_STATE state, bool is_writer);
    ~HOST_INFO();


    int get_port();
    std::string get_host();
    std::string get_host_port_pair();
    bool equal_host_port_pair(HOST_INFO& hi);
    HOST_STATE get_host_state();
    void set_host_state(HOST_STATE state);
    bool is_host_up();
    bool is_host_down();
    bool is_host_writer();
    void mark_as_writer(bool writer);
    static bool is_host_same(const std::shared_ptr<HOST_INFO>& h1, const std::shared_ptr<HOST_INFO>& h2);
    static constexpr int NO_PORT = -1;

    // used to be properties - TODO - remove the ones that are not necessary
    std::string session_id;
    std::string last_updated;
    std::string replica_lag;
    std::string instance_name;

private:
    const std::string HOST_PORT_SEPARATOR = ":";
    const std::string host;
    const int port = NO_PORT;

    HOST_STATE host_state;
    bool is_writer;
};

#endif /* __HOSTINFO_H__ */
