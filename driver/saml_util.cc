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

#include "saml_util.h"
#include <aws/core/auth/AWSCredentials.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/model/AssumeRoleWithSAMLRequest.h>
#include <aws/sts/model/AssumeRoleWithSAMLResult.h>

namespace {
AWS_SDK_HELPER SDK_HELPER;
}

Aws::Auth::AWSCredentials SAML_UTIL::get_aws_credentials(const char* host, const char* region, const char* role_arn,
                                                         const char* idp_arn, const std::string& assertion) {
  ++SDK_HELPER;
  Aws::STS::STSClientConfiguration client_config;

  if (region) {
    client_config.region = region;
  }

  auto sts_client = std::make_shared<Aws::STS::STSClient>(client_config);

  Aws::STS::Model::AssumeRoleWithSAMLRequest sts_req;

  sts_req.SetRoleArn(role_arn);
  sts_req.SetPrincipalArn(idp_arn);
  sts_req.SetSAMLAssertion(assertion);

  const Aws::Utils::Outcome<Aws::STS::Model::AssumeRoleWithSAMLResult, Aws::STS::STSError> outcome =
      sts_client->AssumeRoleWithSAML(sts_req);

  if (!outcome.IsSuccess()) {
    // Returns an empty set of credentials.
    sts_client.reset();
    --SDK_HELPER;
    return Aws::Auth::AWSCredentials();
  }

  const Aws::STS::Model::AssumeRoleWithSAMLResult& result = outcome.GetResult();
  const Aws::STS::Model::Credentials& temp_credentials = result.GetCredentials();
  const auto credentials = Aws::Auth::AWSCredentials(
      temp_credentials.GetAccessKeyId(), temp_credentials.GetSecretAccessKey(), temp_credentials.GetSessionToken());
  sts_client.reset();
  --SDK_HELPER;
  return credentials;
};
