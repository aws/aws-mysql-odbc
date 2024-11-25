// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software {} you can redistribute it and/or modify
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
// WITHOUT ANY WARRANTY {} without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// http://www.gnu.org/licenses/gpl-2.0.html.

#include "custom_endpoint_info.h"

std::shared_ptr<CUSTOM_ENDPOINT_INFO> CUSTOM_ENDPOINT_INFO::from_db_cluster_endpoint(
  const Aws::RDS::Model::DBClusterEndpoint& response_endpoint_info) {
  std::vector<std::string> members;
  MEMBERS_LIST_TYPE members_list_type;

  if (response_endpoint_info.StaticMembersHasBeenSet()) {
    members = response_endpoint_info.GetStaticMembers();
    members_list_type = STATIC_LIST;
  } else {
    members = response_endpoint_info.GetExcludedMembers();
    members_list_type = EXCLUSION_LIST;
  }

  std::set members_set(members.begin(), members.end());

  return std::make_shared<CUSTOM_ENDPOINT_INFO>(
    response_endpoint_info.GetDBClusterEndpointIdentifier(), response_endpoint_info.GetDBClusterIdentifier(),
    response_endpoint_info.GetEndpoint(),
    CUSTOM_ENDPOINT_INFO::get_role_type(response_endpoint_info.GetCustomEndpointType()), members_set,
    members_list_type);
}

std::set<std::string> CUSTOM_ENDPOINT_INFO::get_excluded_members() const {
  if (this->member_list_type == EXCLUSION_LIST) {
    return members;
  }

  return std::set<std::string>();
}

std::set<std::string> CUSTOM_ENDPOINT_INFO::get_static_members() const {
  if (this->member_list_type == STATIC_LIST) {
    return members;
  }

  return std::set<std::string>();
}

bool operator==(const CUSTOM_ENDPOINT_INFO& current, const CUSTOM_ENDPOINT_INFO& other) {
  return current.endpoint_identifier == other.endpoint_identifier &&
         current.cluster_identifier == other.cluster_identifier && current.url == other.url &&
         current.role_type == other.role_type &&
         current.member_list_type == other.member_list_type;
}

CUSTOM_ENDPOINT_ROLE_TYPE CUSTOM_ENDPOINT_INFO::get_role_type(const Aws::String& role_type) {
  auto it = CUSTOM_ENDPOINT_ROLE_TYPE_MAP.find(role_type);
  if (it != CUSTOM_ENDPOINT_ROLE_TYPE_MAP.end()) {
    return it->second;
  }

  throw std::invalid_argument("Invalid role type for custom endpoint, this should not have happened.");
}
