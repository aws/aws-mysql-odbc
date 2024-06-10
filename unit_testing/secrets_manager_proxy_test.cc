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

#include <aws/core/utils/Outcome.h>
#include <aws/secretsmanager/model/GetSecretValueRequest.h>
#include <aws/secretsmanager/SecretsManagerServiceClientModel.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using testing::_;
using testing::InSequence;
using testing::Property;
using testing::Return;
using testing::StrEq;

using namespace Aws::SecretsManager;

namespace {
    const Aws::String TEST_SECRET_ID{"secret_ID"};
    const Aws::String TEST_REGION{"us-east-2"};
    const Aws::String TEST_USERNAME{"test-user"};
    const Aws::String TEST_PASSWORD{"test-password"};
    const std::pair<Aws::String, Aws::String> SECRET_CACHE_KEY = std::make_pair(TEST_SECRET_ID, TEST_REGION);
    const auto TEST_SECRET_STRING = R"({"username": ")" + TEST_USERNAME + R"(", "password": ")" + TEST_PASSWORD +
        R"("})";
    const auto TEST_SECRET = Aws::Utils::Json::JsonValue(TEST_SECRET_STRING);
    const char* TEST_HOST = "test-domain";
    const auto INVALID_SECRET_STRING = "{username: invalid, password: invalid}";
}

static SQLHENV env;
static Aws::SDKOptions sdk_options;

class SecretsManagerProxyTest : public testing::Test {
protected:
    DBC* dbc;
    DataSource* ds;
    MOCK_CONNECTION_PROXY* mock_connection_proxy;
    std::shared_ptr<MOCK_SECRETS_MANAGER_CLIENT> mock_sm_client;

    static void SetUpTestSuite() {
        Aws::InitAPI(sdk_options);
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    }

    static void TearDownTestSuite() {
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        Aws::ShutdownAPI(sdk_options);
    }

    void SetUp() override {
        SQLHDBC hdbc = nullptr;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
        dbc = static_cast<DBC*>(hdbc);

        ds_setattr_from_utf8(&ds->auth_region, (SQLCHAR*)TEST_REGION.c_str());
        ds_setattr_from_utf8(&ds->auth_secret_id, (SQLCHAR*)TEST_SECRET_ID.c_str());

        mock_sm_client = std::make_shared<MOCK_SECRETS_MANAGER_CLIENT>();

        mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
    }

    void TearDown() override {
        TEST_UTILS::get_secrets_cache().clear();
        cleanup_odbc_handles(nullptr, dbc, ds);
    }
};

TEST_F(SecretsManagerProxyTest, NullDBC) {
    EXPECT_THROW(SECRETS_MANAGER_PROXY sm_proxy(nullptr, ds, mock_connection_proxy, mock_sm_client),
                 std::runtime_error);
    delete mock_connection_proxy;
}

TEST_F(SecretsManagerProxyTest, NullDS) {
    EXPECT_THROW(SECRETS_MANAGER_PROXY sm_proxy(dbc, nullptr, mock_connection_proxy, mock_sm_client),
                 std::runtime_error);
    delete mock_connection_proxy;
}

// The proxy will successfully open a connection with a cached secret.
TEST_F(SecretsManagerProxyTest, TestConnectWithCachedSecrets) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    TEST_UTILS::get_secrets_cache().insert({SECRET_CACHE_KEY, TEST_SECRET});

    EXPECT_CALL(*mock_sm_client, GetSecretValue(_)).Times(0);
    EXPECT_CALL(*mock_connection_proxy,
                connect(StrEq(TEST_HOST), StrEq(TEST_USERNAME), StrEq(TEST_PASSWORD), nullptr, 0, nullptr, 0)).
        WillOnce(Return(true));

    const auto ret = sm_proxy.connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);

    EXPECT_EQ(1, TEST_UTILS::get_secrets_cache().size());
    EXPECT_TRUE(ret);
}

// The proxy will attempt to open a connection with an empty secret cache.
// The proxy will fetch the secret from the AWS Secrets Manager.
TEST_F(SecretsManagerProxyTest, TestConnectWithNewSecrets) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    const auto expected_result = Model::GetSecretValueResult().WithSecretString(TEST_SECRET_STRING);
    const auto expected_outcome = Model::GetSecretValueOutcome(expected_result);

    EXPECT_CALL(*mock_sm_client,
                GetSecretValue(Property("GetSecretId", &Aws::SecretsManager::Model::GetSecretValueRequest::GetSecretId,
                    StrEq(TEST_SECRET_ID)))).WillOnce(Return(expected_outcome));
    EXPECT_CALL(*mock_connection_proxy,
                connect(StrEq(TEST_HOST), StrEq(TEST_USERNAME), StrEq(TEST_PASSWORD), nullptr, 0, nullptr, 0)).
        WillOnce(Return(true));

    const auto ret = sm_proxy.connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);

    EXPECT_EQ(1, TEST_UTILS::get_secrets_cache().size());
    EXPECT_TRUE(ret);
}

// The proxy will attempt to open a connection with a cached secret, but it will fail with a generic error.
// In this case, the proxy will return the error back to the user.
TEST_F(SecretsManagerProxyTest, TestFailedInitialConnectionWithUnhandledError) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    TEST_UTILS::get_secrets_cache().insert({SECRET_CACHE_KEY, TEST_SECRET});

    EXPECT_CALL(*mock_sm_client, GetSecretValue(_)).Times(0);
    EXPECT_CALL(*mock_connection_proxy,
                connect(StrEq(TEST_HOST), StrEq(TEST_USERNAME), StrEq(TEST_PASSWORD), nullptr, 0, nullptr, 0)).
        WillOnce(Return(false));
    EXPECT_CALL(*mock_connection_proxy, error_code()).WillOnce(Return(ER_BAD_HOST_ERROR));

    const auto ret = sm_proxy.connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);
    EXPECT_FALSE(ret);
}

// The proxy will attempt to open a connection with a cached secret, but it will fail with an access error.
// In this case, the proxy will fetch the secret and will retry the connection.
TEST_F(SecretsManagerProxyTest, TestConnectWithNewSecretsAfterTryingWithCachedSecrets) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    TEST_UTILS::get_secrets_cache().insert({SECRET_CACHE_KEY, TEST_SECRET});

    const auto expected_result = Model::GetSecretValueResult().WithSecretString(TEST_SECRET_STRING);
    const auto expected_outcome = Model::GetSecretValueOutcome(expected_result);

    EXPECT_CALL(*mock_sm_client,
                GetSecretValue(Property("GetSecretId", &Aws::SecretsManager::Model::GetSecretValueRequest::GetSecretId,
                    StrEq(TEST_SECRET_ID)))).WillOnce(Return(expected_outcome));
    {
        InSequence s;

        EXPECT_CALL(*mock_connection_proxy,
                    connect(StrEq(TEST_HOST), StrEq(TEST_USERNAME), StrEq(TEST_PASSWORD), nullptr, 0, nullptr, 0)).
            WillOnce(Return(false));
        EXPECT_CALL(*mock_connection_proxy, error_code()).WillOnce(Return(ER_ACCESS_DENIED_ERROR));
        EXPECT_CALL(*mock_connection_proxy,
                    connect(StrEq(TEST_HOST), StrEq(TEST_USERNAME), StrEq(TEST_PASSWORD), nullptr, 0, nullptr, 0)).
            WillOnce(Return(true));
    }

    const auto ret = sm_proxy.connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);

    EXPECT_EQ(1, TEST_UTILS::get_secrets_cache().size());
    EXPECT_TRUE(ret);
}

// The proxy will attempt to open a connection after fetching a secret,
// but it will fail because the returned secret could not be parsed.
TEST_F(SecretsManagerProxyTest, TestFailedToReadSecrets) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    const auto expected_result = Model::GetSecretValueResult().WithSecretString(INVALID_SECRET_STRING);
    const auto expected_outcome = Model::GetSecretValueOutcome(expected_result);

    EXPECT_CALL(*mock_sm_client,
                GetSecretValue(Property("GetSecretId", &Aws::SecretsManager::Model::GetSecretValueRequest::GetSecretId,
                    StrEq(TEST_SECRET_ID)))).WillOnce(Return(expected_outcome));

    EXPECT_CALL(*mock_connection_proxy, connect(_, _, _, _, _, _, _)).Times(0);

    const auto ret = sm_proxy.connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);
    EXPECT_FALSE(ret);
    EXPECT_EQ(0, TEST_UTILS::get_secrets_cache().size());
}

// The proxy will attempt to open a connection after fetching a secret,
// but it will fail because the outcome from the AWS Secrets Manager is not successful.
TEST_F(SecretsManagerProxyTest, TestFailedToGetSecrets) {
    SECRETS_MANAGER_PROXY sm_proxy(dbc, ds, mock_connection_proxy, mock_sm_client);

    const auto unsuccessful_outcome = Model::GetSecretValueOutcome();

    EXPECT_CALL(*mock_sm_client,
                GetSecretValue(Property("GetSecretId", &Aws::SecretsManager::Model::GetSecretValueRequest::GetSecretId,
                    StrEq(TEST_SECRET_ID)))).WillOnce(Return(unsuccessful_outcome));

    EXPECT_CALL(*mock_connection_proxy, connect(_, _, _, _, _, _, _)).Times(0);

    const auto ret = sm_proxy.connect(TEST_HOST, nullptr, nullptr, nullptr, 0, nullptr, 0);
    EXPECT_FALSE(ret);
    EXPECT_EQ(0, TEST_UTILS::get_secrets_cache().size());
}

TEST_F(SecretsManagerProxyTest, ParseRegionFromSecret) {
    std::string region = "";
    EXPECT_TRUE(TEST_UTILS::try_parse_region_from_secret(
        "arn:aws:secretsmanager:us-east-1:123456789012:secret:MyTestDatabaseSecret-a1b2c3", region));
    EXPECT_EQ("us-east-1", region);

    region = "";
    EXPECT_TRUE(TEST_UTILS::try_parse_region_from_secret(
        "arn:aws:secretsmanager:us-west-3:0987654321:Whatever", region));
    EXPECT_EQ("us-west-3", region);

    region = "";
    EXPECT_FALSE(TEST_UTILS::try_parse_region_from_secret(
        "arn:aws:secretmanager:us-east-1:123456789012:secret:MyTestDatabaseSecret-a1b2c3", region));
    EXPECT_EQ("", region);

    delete mock_connection_proxy;
}
