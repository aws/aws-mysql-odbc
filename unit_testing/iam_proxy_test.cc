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

static Aws::SDKOptions options;
static MOCK_IAM_PROXY* mock_proxy;

class IamProxyTest : public testing::Test {
protected:
    const char* host = "test_host";
    const char* region = "test_region";
    unsigned int port = 3306;
    const char* user = "test_user";
    const char* token = "test_token";

    static void SetUpTestSuite() {
        Aws::InitAPI(options);
        mock_proxy = new MOCK_IAM_PROXY();
    }

    static void TearDownTestSuite() {
        delete mock_proxy;
        Aws::ShutdownAPI(options);
    }

    void SetUp() override {
    }

    void TearDown() override {
        TEST_UTILS::clear_token_cache(mock_proxy);
    }
};

TEST_F(IamProxyTest, TokenExpiration) {
    const unsigned int time_to_expire = 5;
    TOKEN_INFO info = TOKEN_INFO("test_key", time_to_expire);
    EXPECT_FALSE(info.is_expired());

    std::this_thread::sleep_for(std::chrono::seconds(time_to_expire + 1));
    EXPECT_TRUE(info.is_expired());
}

TEST_F(IamProxyTest, TokenGetsCachedAndRetrieved) {
    std::string cache_key = TEST_UTILS::build_cache_key(host, region, port, user);
    EXPECT_FALSE(TEST_UTILS::token_cache_contains_key(cache_key));

    // We should only generate the token once.
    EXPECT_CALL(*mock_proxy, generate_auth_token(host, region, port, user)).WillOnce(Return(token));

    std::string token1 = mock_proxy->get_auth_token(host, region, port, user, 100);

    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(cache_key));

    // This 2nd call to get_auth_token() will retrieve the cached token.
    std::string token2 = mock_proxy->get_auth_token(host, region, port, user, 100);

    EXPECT_EQ(token, token1);
    EXPECT_TRUE(token1 == token2);
}

TEST_F(IamProxyTest, MultipleCachedTokens) {
    // Two separate tokens should be generated.
    EXPECT_CALL(*mock_proxy, generate_auth_token(_, region, port, user))
        .WillOnce(Return(token))
        .WillOnce(Return(token));

    const char* host2 = "test_host2";

    mock_proxy->get_auth_token(host, region, port, user, 100);
    mock_proxy->get_auth_token(host2, region, port, user, 100);

    std::string cache_key1 = TEST_UTILS::build_cache_key(host, region, port, user);
    std::string cache_key2 = TEST_UTILS::build_cache_key(host2, region, port, user);

    EXPECT_NE(cache_key1, cache_key2);

    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(cache_key1));
    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(cache_key2));
}

TEST_F(IamProxyTest, RegenerateTokenAfterExpiration) {
    // We will generate the token twice because the first token will expire before the 2nd call to get_auth_token().
    EXPECT_CALL(*mock_proxy, generate_auth_token(host, region, port, user))
        .WillOnce(Return(token))
        .WillOnce(Return(token));

    const unsigned int time_to_expire = 5;
    mock_proxy->get_auth_token(host, region, port, user, time_to_expire);

    std::string cache_key = TEST_UTILS::build_cache_key(host, region, port, user);
    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(cache_key));

    // Wait for first token to expire.
    std::this_thread::sleep_for(std::chrono::seconds(time_to_expire + 1));
    mock_proxy->get_auth_token(host, region, port, user, time_to_expire);

    EXPECT_TRUE(TEST_UTILS::token_cache_contains_key(cache_key));
}
