// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see 
// http://www.gnu.org/licenses/gpl-2.0.html.

#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "integration_test_utils.h"

char* INTEGRATION_TEST_UTILS::get_env_var(const char* key, char* default_value) {
    char* value = std::getenv(key);
    if (value == nullptr || value == "") {
        return default_value;
    }

    return value;
}

int INTEGRATION_TEST_UTILS::str_to_int(const char* str) {
    const long int x = strtol(str, nullptr, 10);
    assert(x <= INT_MAX);
    assert(x >= INT_MIN);
    return static_cast<int>(x);
}

std::string INTEGRATION_TEST_UTILS::host_to_IP(std::string hostname) {
    int status;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    char ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname.c_str(), NULL, &hints, &servinfo)) != 0) {
        ADD_FAILURE() << "The IP address of host " << hostname << " could not be determined."
            << "getaddrinfo error:" << gai_strerror(status);
        return {};
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        void* addr;

        struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
        addr = &(ipv4->sin_addr);
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    }

    freeaddrinfo(servinfo);
    return std::string(ipstr);
}
