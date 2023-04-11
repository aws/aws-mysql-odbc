<#
Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0
(GPLv2), as published by the Free Software Foundation, with the
following additional permissions:

This program is distributed with certain software that is licensed
under separate terms, as designated in a particular file or component
or in the license documentation. Without limiting your rights under
the GPLv2, the authors of this program hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with the program.

Without limiting the foregoing grant of rights under the GPLv2 and
additional permission as to separately licensed software, this
program is also subject to the Universal FOSS Exception, version 1.0,
a copy of which can be found along with its FAQ at
http://oss.oracle.com/licenses/universal-foss-exception.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see 
http://www.gnu.org/licenses/gpl-2.0.html.
/#>

$CURRENT_DIR = (Get-Location).Path
$SRC_DIR = "${PSScriptRoot}\..\aws_sdk\aws_sdk_cpp"
$BUILD_DIR = "${SRC_DIR}\..\build"
$INSTALL_DIR = "${BUILD_DIR}\..\install"

$WIN_ARCH = $args[0]
$CONFIGURATION = $args[1]
$BUILD_SHARED_LIBS = $args[2]
$GENERATOR = $args[3]

Write-Host $args

# Make AWS SDK source directory
New-Item -Path $SRC_DIR -ItemType Directory -Force | Out-Null
# Clone the AWS SDK CPP repo
git clone --recurse-submodules -b "1.11.21" "https://github.com/aws/aws-sdk-cpp.git" $SRC_DIR

# Make and move to build directory
New-Item -Path $BUILD_DIR -ItemType Directory -Force | Out-Null
Set-Location $BUILD_DIR

# Configure and build 
cmake $SRC_DIR `
    -A $WIN_ARCH `
    -G $GENERATOR `
    -D TARGET_ARCH="WINDOWS" `
    -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR `
    -D CMAKE_BUILD_TYPE=$CONFIGURATION `
    -D BUILD_ONLY="rds;secretsmanager" `
    -D ENABLE_TESTING="OFF" `
    -D BUILD_SHARED_LIBS=$BUILD_SHARED_LIBS `
    -D CPP_STANDARD="17"

# Build AWS SDK and install to $INSTALL_DIR 
msbuild ALL_BUILD.vcxproj /m /p:Configuration=$CONFIGURATION
msbuild INSTALL.vcxproj /m /p:Configuration=$CONFIGURATION

Set-Location $CURRENT_DIR
