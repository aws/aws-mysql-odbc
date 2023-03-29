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
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/secretsmanager/SecretsManagerClient.h>

#include <gtest/gtest.h>

#include "connection_string_builder.cc"

static Aws::SDKOptions options;

class SecretsManagerIntegrationTest : public testing::Test {
protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");
  Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY),
                                                                    Aws::String(SECRET_ACCESS_KEY),
                                                                    Aws::String(SESSION_TOKEN));
  static void SetUpTestSuite() {
    Aws::InitAPI(options);
  }

  static void TearDownTestSuite() {
    Aws::ShutdownAPI(options);
  }     

   void SetUp() override {}
    
   void TearDown() override {}

};

