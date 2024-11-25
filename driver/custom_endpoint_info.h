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

#ifndef __CUSTOM_ENDPOINT_INFO_H__
#define __CUSTOM_ENDPOINT_INFO_H__

#include <aws/rds/model/DBClusterEndpoint.h>

#include <set>
#include <sstream>
#include <utility>

#include "MYODBC_MYSQL.h"
#include "mylog.h"

/**
 * Enum representing the possible roles of instances specified by a custom endpoint. Note that, currently, it is not
 * possible to create a WRITER custom endpoint.
 */
enum CUSTOM_ENDPOINT_ROLE_TYPE {
  ANY,     // Instances in the custom endpoint may be either a writer or a reader.
  WRITER,  // Instance in the custom endpoint is always the writer.
  READER   // Instances in the custom endpoint are always readers.
};

static std::unordered_map<std::string, CUSTOM_ENDPOINT_ROLE_TYPE> const CUSTOM_ENDPOINT_ROLE_TYPE_MAP = {
  {"ANY", ANY}, {"WRITER", WRITER}, {"READER", READER}};

static std::unordered_map<CUSTOM_ENDPOINT_ROLE_TYPE, std::string> const CUSTOM_ENDPOINT_ROLE_TYPE_STR_MAP = {
  {ANY, "ANY"}, {WRITER, "WRITER"}, {READER, "READER"}};

/**
 * Enum representing the member list type of a custom endpoint. This information can be used together with a member list
 * to determine which instances are included or excluded from a custom endpoint.
 */
enum MEMBERS_LIST_TYPE {
  /**
   * The member list for the custom endpoint specifies which instances are included in the custom endpoint. If new
   * instances are added to the cluster, they will not be automatically added to the custom endpoint.
   */
  STATIC_LIST,
  /**
   * The member list for the custom endpoint specifies which instances are excluded from the custom endpoint. If new
   * instances are added to the cluster, they will be automatically added to the custom endpoint.
   */
  EXCLUSION_LIST
};

static std::unordered_map<MEMBERS_LIST_TYPE, std::string> const MEMBERS_LIST_TYPE_MAP = {
  {STATIC_LIST, "STATIC_LIST0"}, {EXCLUSION_LIST, "EXCLUSION_LIST"}};

class CUSTOM_ENDPOINT_INFO {
 public:
  CUSTOM_ENDPOINT_INFO(std::string endpoint_identifier, std::string cluster_identifier, std::string url,
                       CUSTOM_ENDPOINT_ROLE_TYPE role_type, std::set<std::string> members,
                       MEMBERS_LIST_TYPE member_list_type)
    : endpoint_identifier(std::move(endpoint_identifier)),
      cluster_identifier(std::move(cluster_identifier)),
      url(std::move(url)),
      role_type(role_type),
      members(std::move(members)),
      member_list_type(member_list_type){};
  ~CUSTOM_ENDPOINT_INFO() = default;

  static std::shared_ptr<CUSTOM_ENDPOINT_INFO> from_db_cluster_endpoint(
    const Aws::RDS::Model::DBClusterEndpoint& response_endpoint_info);
  std::string get_endpoint_identifier() const { return this->endpoint_identifier; };
  std::string get_cluster_identifier() const { return this->cluster_identifier; };
  std::string get_url() const { return this->url; };
  CUSTOM_ENDPOINT_ROLE_TYPE get_custom_endpoint_type() const { return this->role_type; };
  MEMBERS_LIST_TYPE get_member_list_type() const { return this->member_list_type; };
  std::set<std::string> get_excluded_members() const;
  std::set<std::string> get_static_members() const;

  std::string to_string() const {
    char buf[4096];
    std::string members_list;

    for (auto const& m : members) {
      members_list += m;
      members_list += ",";
    }
    if (members_list.empty()) {
      members_list = "<no members>";
    } else {
      members_list.pop_back();
    }

    myodbc_snprintf(
      buf, sizeof(buf),
      "CustomEndpointInfo[url=%s, cluster_identifier=%s, custom_endpoint_type=%s, member_list_type=%s, members=[%s]",
      this->url.c_str(), this->cluster_identifier.c_str(),
      CUSTOM_ENDPOINT_ROLE_TYPE_STR_MAP.at(this->role_type).c_str(),
      MEMBERS_LIST_TYPE_MAP.at(this->member_list_type).c_str(), members_list.c_str());

    return std::string(buf);
  }

  friend bool operator==(const CUSTOM_ENDPOINT_INFO& current, const CUSTOM_ENDPOINT_INFO& other);

 private:
  const std::string endpoint_identifier;
  const std::string cluster_identifier;
  const std::string url;
  const CUSTOM_ENDPOINT_ROLE_TYPE role_type;
  const std::set<std::string> members;
  const MEMBERS_LIST_TYPE member_list_type;
  static CUSTOM_ENDPOINT_ROLE_TYPE get_role_type(const Aws::String& role_type);
};

#endif
