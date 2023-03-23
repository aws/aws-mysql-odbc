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

#include "driver/secrets_manager_proxy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using testing::_;
using testing::Return;
using testing::StrEq;

namespace {
    const Aws::String TEST_SECRET_ID{"secret_ID"};
    const Aws::String TEST_REGION{ "us-east-2" };
    const Aws::String TEST_USERNAME{"test-user"};
    const Aws::String TEST_PASSWORD{ "test-password" };
    const std::pair<Aws::String, Aws::String> SECRET_CACHE_KEY = std::make_pair(TEST_SECRET_ID, TEST_REGION);
    Aws::Utils::Json::JsonValue TEST_SECRET;
    const char* TEST_HOST = "test-domain";

}

static Aws::SDKOptions sdk_options;

class SecretsManagerProxyTest : public testing::Test {
protected:
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;
    MOCK_CONNECTION_PROXY* mock_connection_proxy;
    std::shared_ptr<MOCK_SECRETS_MANAGER_CLIENT> mock_sm_client;

    static void SetUpTestSuite() {
        Aws::InitAPI(sdk_options);
    }

    static void TearDownTestSuite() {
        Aws::ShutdownAPI(sdk_options);
        mysql_library_end();
    }

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);
        ds_setattr_from_utf8(&ds->auth_region, (SQLCHAR*)TEST_REGION.c_str());
        ds_setattr_from_utf8(&ds->auth_secret_id, (SQLCHAR*)TEST_SECRET_ID.c_str());

        TEST_SECRET.WithString("username", TEST_USERNAME).WithString("password", TEST_PASSWORD);

        TEST_UTILS::get_secrets_cache().insert({ SECRET_CACHE_KEY, TEST_SECRET });

        mock_sm_client = std::make_shared<MOCK_SECRETS_MANAGER_CLIENT>();

        mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
    }

    void TearDown() override {
        TEST_UTILS::get_secrets_cache().clear();
        cleanup_odbc_handles(env, dbc, ds);
    }
};

TEST_F(SecretsManagerProxyTest, NullDBC) {
    EXPECT_THROW(SECRETS_MANAGER_PROXY sm_proxy(nullptr, ds, mock_connection_proxy, mock_sm_client), std::runtime_error);
    delete mock_connection_proxy;
}

TEST_F(SecretsManagerProxyTest, NullDS) {
    EXPECT_THROW(SECRETS_MANAGER_PROXY sm_proxy(dbc, nullptr, mock_connection_proxy, mock_sm_client), std::runtime_error);
    delete mock_connection_proxy;
}

TEST_F(SecretsManagerProxyTest, TestConnectWithCachedSecrets) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    EXPECT_CALL(*mock_sm_client, GetSecretValue(_)).Times(0);
    EXPECT_CALL(*mock_connection_proxy, real_connect(StrEq(TEST_HOST), StrEq(TEST_USERNAME), StrEq(TEST_PASSWORD), nullptr, 0, nullptr, 0)).Times(1);
    EXPECT_CALL(*mock_connection_proxy, error_code()).WillOnce(Return(0));

    sm_proxy.real_connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);

    EXPECT_EQ(1, TEST_UTILS::get_secrets_cache().size());
}


