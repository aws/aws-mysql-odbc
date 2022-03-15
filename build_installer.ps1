<#
 * AWS JDBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
/#>

<#
This script is to assist with building the driver and creating the installer. There are two optional arguments:
- $CONFIGURATION: determines what configuration you would like to build the driver in (Debug, Release, RelWithDebInfo)
- $MYSQL_DIR: determines where your MySQL installation is located

The current default behaviour is to build the driver and installer without unit or integration tests on the Release configuration, 
with "C:\Program Files\MySQL\MySQL Server 8.0" set as the location of the MySQL directory

Note that building the installer requires the following:
- Wix 3.0 or above (https://wixtoolset.org/)
- Microsoft Visual Studio environment
- CMake 2.4.6 (http://www.cmake.org)
#>

$CONFIGURATION = $args[0]
$MYSQL_DIR = $args[1]

# Set default values
if ($null -eq $CONFIGURATION) {
    $CONFIGURATION = "Release"
}
if ($null -eq $MYSQL_DIR) {
    $MYSQL_DIR = "C:\Program Files\MySQL\MySQL Server 8.0"
}

# BUILD DRIVER
cmake -S . -G "Visual Studio 16 2019" -DMYSQL_DIR="$MYSQL_DIR" -DMYSQLCLIENT_STATIC_LINKING=TRUE
cmake --build . --config "$CONFIGURATION"

# CREATE INSTALLER
# Copy dll, installer, and info files to wix folder
Copy-Item .\lib\$CONFIGURATION\myodbc8*.dll .\Wix\x64
Copy-Item .\lib\$CONFIGURATION\myodbc8*.lib .\Wix\x64
Copy-Item .\lib\$CONFIGURATION\myodbc8*.dll .\Wix\x86
Copy-Item .\lib\$CONFIGURATION\myodbc8*.lib .\Wix\x86
Copy-Item .\bin\$CONFIGURATION\myodbc-installer.exe .\Wix\x64
Copy-Item .\bin\$CONFIGURATION\myodbc-installer.exe .\Wix\x86
Copy-Item .\INFO_BIN .\Wix\doc
Copy-Item .\INFO_SRC .\Wix\doc
Copy-Item .\ChangeLog .\Wix\doc
Copy-Item .\README.md .\Wix\doc

Set-Location .\Wix
cmake -DMSI_64=1 -G "NMake Makefiles"
nmake

Set-Location ..\
