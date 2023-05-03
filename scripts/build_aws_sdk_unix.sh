
#Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License, version 2.0
#(GPLv2), as published by the Free Software Foundation, with the
#following additional permissions:

#This program is distributed with certain software that is licensed
#under separate terms, as designated in a particular file or component
#or in the license documentation. Without limiting your rights under
#the GPLv2, the authors of this program hereby grant you an additional
#permission to link the program and your derivative works with the
#separately licensed software that they have included with the program.

#Without limiting the foregoing grant of rights under the GPLv2 and
#additional permission as to separately licensed software, this
#program is also subject to the Universal FOSS Exception, version 1.0,
#a copy of which can be found along with its FAQ at
#http://oss.oracle.com/licenses/universal-foss-exception.

#This program is distributed in the hope that it will be useful, but
#WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#See the GNU General Public License, version 2.0, for more details.

#You should have received a copy of the GNU General Public License
#along with this program. If not, see 
#http://www.gnu.org/licenses/gpl-2.0.html.

CONFIGURATION=$1

# Build AWS SDK

# one liner to get script directory no matter where it being called from
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
AWS_SRC_DIR=$SCRIPT_DIR/../aws_sdk/aws_sdk_cpp
AWS_BUILD_DIR=$AWS_SRC_DIR/../build
AWS_INSTALL_DIR=$AWS_SRC_DIR/../install

mkdir -p $AWS_SRC_DIR $AWS_BUILD_DIR $AWS_INSTALL_DIR

git clone --recurse-submodules -b "1.11.71" "https://github.com/aws/aws-sdk-cpp.git" $AWS_SRC_DIR

cmake -S $AWS_SRC_DIR -B $AWS_BUILD_DIR -DCMAKE_INSTALL_PREFIX="${AWS_INSTALL_DIR}" -DCMAKE_BUILD_TYPE="${CONFIGURATION}" -DBUILD_ONLY="rds;secretsmanager" -DENABLE_TESTING="OFF" -DBUILD_SHARED_LIBS="ON" -DCPP_STANDARD="14"
cd $AWS_BUILD_DIR
make -j 4
make install
