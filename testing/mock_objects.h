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

#ifndef __MOCKOBJECTS_H__
#define __MOCKOBJECTS_H__

#include <gmock/gmock.h>

#include "driver/connection.h"
#include "driver/failover.h"
#include "driver/topology_service.h"

class MOCK_CONNECTION : public CONNECTION_INTERFACE {
 public:
    MOCK_METHOD(bool, is_null, ());
    MOCK_METHOD(bool, is_connected, ());
    MOCK_METHOD(bool, try_execute_query, (const char*));
    MOCK_METHOD(char**, fetch_next_row, ());
    MOCK_METHOD(void, close_connection, ());
};

class MOCK_TOPOLOGY_SERVICE : public TOPOLOGY_SERVICE {
 public:
    MOCK_METHOD(std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, get_topology,
                (std::shared_ptr<CONNECTION_INTERFACE>, bool));
    MOCK_METHOD(void, mark_host_down, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(void, unmark_host_down, (std::shared_ptr<HOST_INFO>));
};

class MOCK_READER_HANDLER : public FAILOVER_READER_HANDLER {
 public:
    MOCK_READER_HANDLER() : FAILOVER_READER_HANDLER(nullptr, nullptr) {}
    MOCK_METHOD(READER_FAILOVER_RESULT, get_reader_connection,
                (std::shared_ptr<CLUSTER_TOPOLOGY_INFO>,
                const std::function<bool()>));
};

class MOCK_CONNECTION_HANDLER : public FAILOVER_CONNECTION_HANDLER {
 public:
    MOCK_CONNECTION_HANDLER() : FAILOVER_CONNECTION_HANDLER(nullptr) {}
    MOCK_METHOD(std::shared_ptr<CONNECTION_INTERFACE>, connect,
                (std::shared_ptr<HOST_INFO>));
};

#endif /* __MOCKOBJECTS_H__ */
