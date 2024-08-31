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

#include <aws/core/Aws.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::Return;

namespace {
    const std::string TEST_HOST{"test_host"};
    const std::string TEST_REGION{"test_region"};
    const std::string TEST_USER{"test_user"};
    const std::string TEST_TOKEN{"test_token"};
    const unsigned int TEST_PORT = 3306;
    const unsigned int TEST_EXPIRATION = 100;
}

static SQLHENV env;
static Aws::SDKOptions options;

class IamProxyTest : public testing::Test {
protected:
    DBC *dbc;
    DataSource *ds;
    MOCK_CONNECTION_PROXY *mock_connection_proxy;
    std::shared_ptr<MOCK_AUTH_UTIL> token_test_auth_util;
    std::unordered_map<std::string, TOKEN_INFO> token_cache;
    std::mutex token_cache_mutex;

    static void SetUpTestSuite() {
        Aws::InitAPI(options);
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    }

    static void TearDownTestSuite() {
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        Aws::ShutdownAPI(options);
    }

    void SetUp() override {
        SQLHDBC hdbc = nullptr;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
        dbc = static_cast<DBC *>(hdbc);
        ds = new DataSource();

        ds->opt_AUTH_HOST.set_remove_brackets((SQLWCHAR*)TEST_HOST.c_str(), TEST_HOST.size());
        ds->opt_AUTH_REGION.set_remove_brackets((SQLWCHAR*)TEST_REGION.c_str(), TEST_REGION.size());
        ds->opt_UID.set_remove_brackets((SQLWCHAR*)TEST_USER.c_str(), TEST_USER.size());
        ds->opt_AUTH_PORT = TEST_PORT;
        ds->opt_AUTH_EXPIRATION = TEST_EXPIRATION;

        mock_connection_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
        token_test_auth_util = std::make_shared<MOCK_AUTH_UTIL>();
    }

    void TearDown() override {
        token_cache.clear();
        cleanup_odbc_handles(nullptr, dbc, ds);
    }
};

TEST_F(IamProxyTest, TokenExpiration) {
    const unsigned int time_to_expire = 5;
    TOKEN_INFO info = TOKEN_INFO("test_key", time_to_expire);
    EXPECT_FALSE(info.is_expired());

    std::this_thread::sleep_for(std::chrono::seconds(time_to_expire + 1));
    EXPECT_TRUE(info.is_expired());

    delete mock_connection_proxy;
}

TEST_F(IamProxyTest, TokenGetsCachedAndRetrieved) {
    std::string cache_key = TEST_UTILS::build_cache_key(
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str());
    EXPECT_FALSE(TEST_UTILS::token_cache_contains_key(token_cache, cache_key));

    // We should only generate the token once.
    EXPECT_CALL(*token_test_auth_util, generate_token(_, _, _, _))
        .WillOnce(Return(TEST_TOKEN));

    std::string token1;
    bool use_cached_bool;
    std::tie(token1, use_cached_bool) = token_test_auth_util->get_auth_token(token_cache, token_cache_mutex,
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str(), 100);

    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(token_cache, cache_key));
    EXPECT_FALSE(use_cached_bool);

    // This 2nd call to get_auth_token() will retrieve the cached token.
    std::string token2;
    std::tie(token2, use_cached_bool) = token_test_auth_util->get_auth_token(
        token_cache, token_cache_mutex, TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str(), 100);

    EXPECT_EQ(TEST_TOKEN, token1);
    EXPECT_TRUE(token1 == token2);
    EXPECT_TRUE(use_cached_bool);
}

TEST_F(IamProxyTest, MultipleCachedTokens) {
    // Two separate tokens should be generated.
    EXPECT_CALL(*token_test_auth_util, generate_token(_, TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str()))
        .WillOnce(Return(TEST_TOKEN))
        .WillOnce(Return(TEST_TOKEN));

    const char *host2 = "test_host2";

    std::string token1;
    bool use_cached_bool;
    std::tie(token1, use_cached_bool) = token_test_auth_util->get_auth_token(token_cache, token_cache_mutex,
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str(), 100);
    std::tie(token1, use_cached_bool) = token_test_auth_util->get_auth_token(
        token_cache, token_cache_mutex, host2, TEST_REGION.c_str(),
                                             TEST_PORT, TEST_USER.c_str(), 100);


    std::string cache_key1 = TEST_UTILS::build_cache_key(
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str());
    std::string cache_key2 = TEST_UTILS::build_cache_key(
        host2, TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str());

    EXPECT_NE(cache_key1, cache_key2);
    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(token_cache, cache_key1));
    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(token_cache, cache_key2));
}

TEST_F(IamProxyTest, RegenerateTokenAfterExpiration) {
    // We will generate the token twice because the first token will expire before the 2nd call to get_auth_token().
    EXPECT_CALL(*token_test_auth_util,
                generate_token(TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str()))
        .WillOnce(Return(TEST_TOKEN))
        .WillOnce(Return(TEST_TOKEN));

    constexpr unsigned int time_to_expire = 5;

    std::string token;
    bool use_cached_bool;
    std::tie(token, use_cached_bool) =
        token_test_auth_util->get_auth_token(token_cache, token_cache_mutex, TEST_HOST.c_str(), TEST_REGION.c_str(),
                                             TEST_PORT, TEST_USER.c_str(), time_to_expire);
    std::string cache_key = TEST_UTILS::build_cache_key(
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str());
    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(token_cache, cache_key));

    // Wait for first token to expire.
    std::this_thread::sleep_for(std::chrono::seconds(time_to_expire + 1));
    std::tie(token, use_cached_bool) =
        token_test_auth_util->get_auth_token(token_cache, token_cache_mutex, TEST_HOST.c_str(), TEST_REGION.c_str(),
                                             TEST_PORT, TEST_USER.c_str(), time_to_expire);

    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(token_cache, cache_key));
}

TEST_F(IamProxyTest, ForceGenerateNewToken) {
    // We expect a token to be generated twice because the 2nd call to get_auth_token forces a fresh token.
    EXPECT_CALL(*token_test_auth_util,
        generate_token(TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str()))
        .WillOnce(Return(TEST_TOKEN))
        .WillOnce(Return(TEST_TOKEN));

    constexpr unsigned int time_to_expire = 100;
    token_test_auth_util->get_auth_token(token_cache, token_cache_mutex,
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str(), time_to_expire);
    
    // 2nd call to get_auth_token should still generate a new token because we are forcing it
    // even though the first token has not yet expired
    token_test_auth_util->get_auth_token(token_cache, token_cache_mutex,
        TEST_HOST.c_str(), TEST_REGION.c_str(), TEST_PORT, TEST_USER.c_str(), time_to_expire, true);
}

TEST_F(IamProxyTest, RetryConnectionWithFreshTokenAfterFailingWithCachedToken) {
    // 1st connect is to get a token cached.
    // 2nd connect is a failed connection using that cached token.
    // 3rd connect is a successful connection using a fresh token.
    EXPECT_CALL(*mock_connection_proxy, connect(_, _, _, _, _, _, _))
        .WillOnce(Return(true))
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    // Only called twice because one of the above connection attempts used a cached token.
    EXPECT_CALL(*token_test_auth_util,
        generate_token(_, _, _, _))
        .WillOnce(Return(TEST_TOKEN))
        .WillOnce(Return(TEST_TOKEN));

    IAM_PROXY iam_proxy(dbc, ds, mock_connection_proxy, token_test_auth_util);

    // First successful connection to get a token cached.
    bool ret = iam_proxy.connect(TEST_HOST.c_str(), TEST_USER.c_str(), "", "", TEST_PORT, "", 0);
    EXPECT_TRUE(ret);

    // This will first try to connect with the token that was cached in the above connection. 
    // After failing that attempt it will try again with a fresh token and succeed.
    ret = iam_proxy.connect(TEST_HOST.c_str(), TEST_USER.c_str(), "", "", TEST_PORT, "", 0);
    EXPECT_TRUE(ret);
}

TEST_F(IamProxyTest, UseRegularPortWhenAuthPortIsNotProvided) {
    ds->opt_AUTH_PORT = UNDEFINED_PORT;
    
    // Verify that we generate the token with the regular port when we do not have auth port.
    EXPECT_CALL(*token_test_auth_util,
        generate_token(_, _, TEST_PORT, _))
        .WillOnce(Return(TEST_TOKEN));

    IAM_PROXY iam_proxy(dbc, ds, mock_connection_proxy, token_test_auth_util);

    iam_proxy.connect(TEST_HOST.c_str(), TEST_USER.c_str(), "", "", TEST_PORT, "", 0);
}
