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

#include "driver/query_parsing.h"

#include <gtest/gtest.h>

class QueryParsingTest : public testing::Test {};

TEST_F(QueryParsingTest, ParseMultiStatementQuery) {
    const char* query =
        "      \r\n  \t \n  start   \t transaction  ;"
        "select \t col\r\nfrom   \t table;"
        " select \"abcd;efgh\";"
        " \r\n\t   select \"abcd;efgh\"    \t 'ijkl;mnop' ;"
        " select \"abcd;efgh 'ijkl' \";"
        " select 'abcd;efgh';;;   \n\r\t ;"
        "select 'abcd;efgh'    \t \"ijkl;mnop\" ;"
        " select 'abcd;efgh \"ijkl\" ';"
        "select    \t  \n \"abcd   \n  \\\"efgh    ;  \t ijkl\\\"  \\\\\"   \t ;"
        "select   \r\n \'abcd   \n  \\\'efgh    ;  \t ijkl\\\'  \\\\\'";
    std::vector<std::string> expected {
        "start transaction",
        "select col from table",
        "select \"abcd;efgh\"",
        "select \"abcd;efgh\" 'ijkl;mnop'",
        "select \"abcd;efgh 'ijkl' \"",
        "select 'abcd;efgh'",
        "select 'abcd;efgh' \"ijkl;mnop\"",
        "select 'abcd;efgh \"ijkl\" '",
        "select \"abcd   \n  \\\"efgh    ;  \t ijkl\\\"  \\\\\"",
        "select \'abcd   \n  \\\'efgh    ;  \t ijkl\\\'  \\\\\'" };

    std::vector<std::string> actual = parse_query_into_statements(query);
    
    ASSERT_EQ(expected.size(), actual.size());
    for (int i = 0; i < actual.size(); i++)
    {
        EXPECT_EQ(expected[i], actual[i]);
    }
}
