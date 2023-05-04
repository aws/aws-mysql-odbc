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

#ifndef __INTEGRATIONTESTUTILS_H__
#define __INTEGRATIONTESTUTILS_H__

#define MAX_NAME_LEN 4096
#define SQL_MAX_MESSAGE_LENGTH 512

#define AS_SQLCHAR(str) const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(str))

class INTEGRATION_TEST_UTILS {
public:
    static char* get_env_var(const char* key, char* default_value);
    static int str_to_int(const char* str);
    static std::string host_to_IP(std::string hostname);
};

#endif /* __INTEGRATIONTESTUTILS_H__ */
