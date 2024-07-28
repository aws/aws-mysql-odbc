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

#include "adfs_proxy.h"
#include "driver.h"

ADFS_PROXY::ADFS_PROXY(DBC* dbc, DataSource* ds) : ADFS_PROXY(dbc, ds, nullptr) {};

ADFS_PROXY::ADFS_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : CONNECTION_PROXY(dbc, ds) {
  this->next_proxy = next_proxy;
  if (ds->opt_AUTH_REGION) {
      this->auth_util = std::make_shared<AUTH_UTIL>((const char*)ds->opt_AUTH_REGION);
  }
  else {
      this->auth_util = std::make_shared<AUTH_UTIL>();
  }
}

#ifdef UNIT_TEST_BUILD
ADFS_PROXY::ADFS_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy,
    std::shared_ptr<AUTH_UTIL> auth_util) : CONNECTION_PROXY(dbc, ds) {
    this->next_proxy = next_proxy;
    this->auth_util = auth_util;
}
#endif

ADFS_PROXY::~ADFS_PROXY() { this->auth_util.reset(); }

bool ADFS_PROXY::connect(const char* host, const char* user, const char* password, const char* database,
                         unsigned int port, const char* socket, unsigned long flags) {
  return true;
}
