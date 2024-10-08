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

#include "driver/efm_proxy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using testing::_;
using testing::Return;

class EFMProxyTest : public testing::Test {
protected:
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;
    std::shared_ptr<MOCK_MONITOR_SERVICE> mock_monitor_service;
    MOCK_CONNECTION_PROXY* mock_connection_proxy;


    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {
        mysql_library_end();
    }

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);

        ds->opt_ENABLE_FAILURE_DETECTION = true;

        mock_monitor_service = std::make_shared<MOCK_MONITOR_SERVICE>();
        mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds);
    }
};

TEST_F(EFMProxyTest, NullDBC) {
    EXPECT_THROW(EFM_PROXY efm_proxy(nullptr, ds, mock_connection_proxy), std::runtime_error);
    delete mock_connection_proxy;
}

TEST_F(EFMProxyTest, NullDS) {
    EXPECT_THROW(EFM_PROXY efm_proxy(dbc, nullptr, mock_connection_proxy), std::runtime_error);
    delete mock_connection_proxy;
}

TEST_F(EFMProxyTest, FailureDetectionDisabled) {
    ds->opt_ENABLE_FAILURE_DETECTION = false;

    EXPECT_CALL(*mock_monitor_service, start_monitoring(_, _, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(*mock_monitor_service, stop_monitoring(_)).Times(0);
    EXPECT_CALL(*mock_connection_proxy, init());
    EXPECT_CALL(*mock_connection_proxy, mock_connection_proxy_destructor());

    EFM_PROXY efm_proxy(dbc, ds, mock_connection_proxy, mock_monitor_service);
    efm_proxy.init();
}

TEST_F(EFMProxyTest, FailureDetectionEnabled) {
    auto mock_context = std::make_shared<MONITOR_CONNECTION_CONTEXT>(
        nullptr, std::set<std::string>(), std::chrono::milliseconds(0),
        std::chrono::milliseconds(0), 0);

    EXPECT_CALL(*mock_monitor_service, start_monitoring(_, _, _, _, _, _, _, _, _)).WillOnce(Return(mock_context));
    EXPECT_CALL(*mock_monitor_service, stop_monitoring(mock_context)).Times(1);
    const char *q = nullptr;
    EXPECT_CALL(*mock_connection_proxy, query(q));
    EXPECT_CALL(*mock_connection_proxy, mock_connection_proxy_destructor());

    EFM_PROXY efm_proxy(dbc, ds, mock_connection_proxy, mock_monitor_service);
    efm_proxy.query(q);
}

TEST_F(EFMProxyTest, DoesNotNeedMonitoring) {
    EXPECT_CALL(*mock_monitor_service, start_monitoring(_, _, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(*mock_monitor_service, stop_monitoring(_)).Times(0);
    EXPECT_CALL(*mock_connection_proxy, close());
    EXPECT_CALL(*mock_connection_proxy, mock_connection_proxy_destructor());

    EFM_PROXY efm_proxy(dbc, ds, mock_connection_proxy, mock_monitor_service);
    efm_proxy.close();
}
