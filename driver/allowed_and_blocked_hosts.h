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

#ifndef __ALLOWED_AND_BLOCKED_HOSTS__
#define __ALLOWED_AND_BLOCKED_HOSTS__

#include <set>
#include <string>

/**
 * Represents the allowed and blocked hosts for connections.
 */
class ALLOWED_AND_BLOCKED_HOSTS {
 public:
  /**
   * Constructs an AllowedAndBlockedHosts instance with the specified allowed and blocked host IDs.
   * @param allowed_host_ids The set of allowed host IDs for connections. If null or empty, all host IDs that are not in
   * `blocked_host_ids` are allowed.
   * @param blocked_host_ids The set of blocked host IDs for connections. If null or empty, all host IDs in
   *                       `allowed_host_ids` are allowed. If `allowed_host_ids` is also null or empty, there
   *                       are no restrictions on which hosts are allowed.
   */
  ALLOWED_AND_BLOCKED_HOSTS(const std::set<std::string>& allowed_host_ids,
                            const std::set<std::string>& blocked_host_ids)
      : allowed_host_ids(allowed_host_ids), blocked_host_ids(blocked_host_ids){};

  /**
   * Returns the set of allowed host IDs for connections. If null or empty, all host IDs that are not in
   * `blocked_host_ids` are allowed.
   *
   * @return the set of allowed host IDs for connections.
   */
  std::set<std::string> get_allowed_host_ids() { return this->allowed_host_ids; };

  /**
   * Returns the set of blocked host IDs for connections. If null or empty, all host IDs in `allowed_host_ids`
   * are allowed. If `allowed_host_ids` is also null or empty, there are no restrictions on which hosts are allowed.
   *
   * @return the set of blocked host IDs for connections.
   */
  std::set<std::string> get_blocked_host_ids() { return this->blocked_host_ids; };

 private:
  std::set<std::string> allowed_host_ids;
  std::set<std::string> blocked_host_ids;
};

#endif
