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

#include "rds_utils.h"

namespace {
const std::regex AURORA_DNS_PATTERN(
    R"#((.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.([a-zA-Z0-9\-]+)\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex AURORA_PROXY_DNS_PATTERN(R"#((.+)\.(proxy-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
                                          std::regex_constants::icase);
const std::regex AURORA_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-|cluster-ro-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex AURORA_WRITER_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#", std::regex_constants::icase);
const std::regex AURORA_READER_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-ro-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#", std::regex_constants::icase);
const std::regex AURORA_CUSTOM_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-custom-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#", std::regex_constants::icase);
const std::regex AURORA_CHINA_DNS_PATTERN(
    R"#((.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
    std::regex_constants::icase);
const std::regex AURORA_CHINA_PROXY_DNS_PATTERN(
    R"#((.+)\.(proxy-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
    std::regex_constants::icase);
const std::regex AURORA_CHINA_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-|cluster-ro-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
    std::regex_constants::icase);
const std::regex AURORA_CHINA_WRITER_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
    std::regex_constants::icase);
const std::regex AURORA_CHINA_READER_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-ro-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
    std::regex_constants::icase);
const std::regex AURORA_CHINA_CUSTOM_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-custom-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
    std::regex_constants::icase);
const std::regex IPV4_PATTERN(
    R"#(^(([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){1}(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){2}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$)#");
const std::regex IPV6_PATTERN(R"#(^[0-9a-fA-F]{1,4}(:[0-9a-fA-F]{1,4}){7}$)#");
const std::regex IPV6_COMPRESSED_PATTERN(
    R"#(^(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)::(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)$)#");
}  // namespace

bool RDS_UTILS::is_dns_pattern_valid(std::string host) { return (host.find("?") != std::string::npos); }

bool RDS_UTILS::is_rds_dns(std::string host) {
  return std::regex_match(host, AURORA_DNS_PATTERN) || std::regex_match(host, AURORA_CHINA_DNS_PATTERN);
}

bool RDS_UTILS::is_rds_cluster_dns(std::string host) {
  return std::regex_match(host, AURORA_CLUSTER_PATTERN) || std::regex_match(host, AURORA_CHINA_CLUSTER_PATTERN);
}

bool RDS_UTILS::is_rds_proxy_dns(std::string host) {
  return std::regex_match(host, AURORA_PROXY_DNS_PATTERN) || std::regex_match(host, AURORA_CHINA_PROXY_DNS_PATTERN);
}

bool RDS_UTILS::is_rds_writer_cluster_dns(std::string host) {
  return std::regex_match(host, AURORA_WRITER_CLUSTER_PATTERN) ||
         std::regex_match(host, AURORA_CHINA_WRITER_CLUSTER_PATTERN);
}

bool RDS_UTILS::is_rds_reader_cluster_dns(std::string host) {
  return std::regex_match(host, AURORA_READER_CLUSTER_PATTERN) ||
         std::regex_match(host, AURORA_CHINA_READER_CLUSTER_PATTERN);
}

bool RDS_UTILS::is_rds_custom_cluster_dns(std::string host) {
  return std::regex_match(host, AURORA_CUSTOM_CLUSTER_PATTERN) ||
         std::regex_match(host, AURORA_CHINA_CUSTOM_CLUSTER_PATTERN);
}

#if defined(__APPLE__) || defined(__linux__)
#define strcmp_case_insensitive(str1, str2) strcasecmp(str1, str2)
#else
#define strcmp_case_insensitive(str1, str2) strcmpi(str1, str2)
#endif

std::string RDS_UTILS::get_rds_cluster_host_url(std::string host) {
  auto f = [host](const std::regex pattern) {
    std::smatch m;
    if (std::regex_search(host, m, pattern) && m.size() > 1) {
      std::string gr1 = m.size() > 1 ? m.str(1) : std::string("");
      std::string gr2 = m.size() > 2 ? m.str(2) : std::string("");
      std::string gr3 = m.size() > 3 ? m.str(3) : std::string("");
      if (!gr1.empty() && !gr3.empty() &&
          (strcmp_case_insensitive(gr2.c_str(), "cluster-") == 0 ||
           strcmp_case_insensitive(gr2.c_str(), "cluster-ro-") == 0)) {
        std::string result;
        result.assign(gr1);
        result.append(".cluster-");
        result.append(gr3);

        return result;
      }
    }
    return std::string();
  };

  auto result = f(AURORA_CLUSTER_PATTERN);
  if (!result.empty()) {
    return result;
  }

  return f(AURORA_CHINA_CLUSTER_PATTERN);
}

std::string RDS_UTILS::get_rds_cluster_id(std::string host) {
  auto f = [host](const std::regex pattern) {
    std::smatch m;
    if (std::regex_search(host, m, pattern) && m.size() > 1 && !m.str(2).empty()) {
      return m.str(1);
    }
    return std::string();
  };

  auto result = f(AURORA_DNS_PATTERN);
  if (!result.empty()) {
    return result;
  }

  return f(AURORA_CHINA_DNS_PATTERN);
}

std::string RDS_UTILS::get_rds_instance_host_pattern(std::string host) {
  auto f = [host](const std::regex pattern) {
    std::smatch m;
    if (std::regex_search(host, m, pattern) && m.size() > 4 && !m.str(3).empty()) {
      std::string result("?.");
      result.append(m.str(3));

      return result;
    }
    return std::string();
  };

  auto result = f(AURORA_DNS_PATTERN);
  if (!result.empty()) {
    return result;
  }

  return f(AURORA_CHINA_DNS_PATTERN);
}

std::string RDS_UTILS::get_rds_region(std::string host) {
  auto f = [host](const std::regex pattern) {
    std::smatch m;
    if (std::regex_search(host, m, pattern) && m.size() > 4 && !m.str(4).empty()) {
      return m.str(4);
    }
    return std::string();
  };

  auto result = f(AURORA_DNS_PATTERN);
  if (!result.empty()) {
    return result;
  }

  return f(AURORA_CHINA_DNS_PATTERN);
}

bool RDS_UTILS::is_ipv4(std::string host) { return std::regex_match(host, IPV4_PATTERN); }
bool RDS_UTILS::is_ipv6(std::string host) {
  return std::regex_match(host, IPV6_PATTERN) || std::regex_match(host, IPV6_COMPRESSED_PATTERN);
}
