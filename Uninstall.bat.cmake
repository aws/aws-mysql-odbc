@ECHO OFF
REM Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved. 
REM 
REM This program is free software; you can redistribute it and/or modify 
REM it under the terms of the GNU General Public License, version 2.0, as 
REM published by the Free Software Foundation. 
REM
REM This program is also distributed with certain software (including 
REM but not limited to OpenSSL) that is licensed under separate terms, 
REM as designated in a particular file or component or in included license 
REM documentation. The authors of MySQL hereby grant you an 
REM additional permission to link the program and your derivative works 
REM with the separately licensed software that they have included with 
REM MySQL. 
REM 
REM Without limiting anything contained in the foregoing, this file, 
REM which is part of MySQL Server, is also subject to the 
REM Universal FOSS Exception, version 1.0, a copy of which can be found at 
REM http://oss.oracle.com/licenses/universal-foss-exception. 
REM 
REM This program is distributed in the hope that it will be useful, but 
REM WITHOUT ANY WARRANTY; without even the implied warranty of 
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
REM See the GNU General Public License, version 2.0, for more details. 
REM 
REM You should have received a copy of the GNU General Public License 
REM along with this program; if not, write to the Free Software Foundation, Inc., 
REM 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA 

REM #########################################################
REM
REM \brief  Deregister Connector/ODBC driver registered with
REM         Install.bat
REM
REM         This exists for those working with the Windows 
REM         source distribution or with installer-less 
REM         binary distribution.
REM
REM         If driver was registerd under non-default name,
REM         the name used should be specified as first 
REM         parameter of this script.
REM
REM         Note that you should manually remove all data
REM         sources using a driver before uninstalling that
REM         driver.
REM
REM \sa     INSTALL.win
REM
REM #########################################################

REM # SETLOCAL prevents the variables set in this script to
REM # be exported to the environment and pollute it
SETLOCAL

SET    installer=myodbc-installer
SET    driver_found=no
SET    driver_name=none

IF "%~1" == "" GOTO :doFindInstaller
SET  driver_name=%~1

:doFindInstaller
REM # Find the installer utility

SET bindir=none
FOR %%G IN (. bin bin\release bin\relwithdebinfo bin\debug) DO CALL :subFindInstaller %%G

SET myodbc_installer=%bindir%\%installer%.exe
IF NOT "%bindir%" == "none" GOTO :lookup_deregister

REM # Try if it is in the path
SET myodbc_installer=%installer%.exe
"%myodbc_installer%" >nul 2>nul
REM # "Command not found" generates error 9009
IF NOT ERRORLEVEL 9000 DO GOTO :lookup_deregister

GOTO :errorNoInstaller

REM ######
REM # A subroutine to check if given location
REM # (relative to working dir) contains myodbc
REM # installer utility.
REM ######
:subFindInstaller
REM # Skip check if a good libdir was already found
IF NOT "%bindir%" == "none" GOTO :eof
SET bindir=%CD%\%1
IF NOT EXIST "%bindir%\%installer%.exe"  GOTO :wrongBinDir
REM ECHO Bindir (%bindir%) is OK.
GOTO :eof

:wrongBinDir
REM ECHO Bindir (%bindir%) is wrong.
SET bindir=none
GOTO :eof

:lookup_deregister
IF "%driver_name%" == "none" FOR %%d IN (@CONNECTOR_DRIVER_TYPE@) DO CALL :driverLookup "MySQL ODBC @CONNECTOR_MAJOR@.@CONNECTOR_MINOR@ %%d% Driver"
IF NOT "%driver_name%" == "none" CALL :driverLookup "%driver_name%"

IF %driver_found% == yes GOTO doSuccess

ECHO ^+-----------------------------------------------------^+
ECHO ^| ERROR                                               ^|
ECHO ^+-----------------------------------------------------^+
ECHO ^|                                                     ^|
ECHO ^| No driver was found and deregistered                 ^|
ECHO ^|                                                     ^|
ECHO ^+-----------------------------------------------------^+

EXIT /B 1

:driverLookup
ECHO installer = %myodbc_installer%
ECHO driver = %1

REM # Check if driver is registered

"%myodbc_installer%" -d -l -n %1  2>nul
IF ERRORLEVEL 0 IF NOT ERRORLEVEL 1 CALL :doDeregister %1
IF ERRORLEVEL 1 CALL :errorNotRegistered

GOTO :eof

:doDeregister

ECHO Deregistering %1
"%myodbc_installer%" -d -r -n %1
SET driver_found=yes

GOTO :eof

:doSuccess
ECHO ^+-----------------------------------------------------^+
ECHO ^| DONE                                                ^|
ECHO ^+-----------------------------------------------------^+
ECHO ^|                                                     ^|
ECHO ^| Hopefully things went well; the Connector/ODBC      ^|
ECHO ^| driver has been deregistered.                       ^|
ECHO ^|                                                     ^|
ECHO ^+-----------------------------------------------------^+
EXIT /B 0

:errorNoInstaller
ECHO ^+-----------------------------------------------------^+
ECHO ^| ERROR                                               ^|
ECHO ^+-----------------------------------------------------^+
ECHO ^|                                                     ^|
ECHO ^| Could not find the MyODBC Installer utility. Run    ^|
ECHO ^| this script from the installation directory.        ^|
ECHO ^|                                                     ^|
ECHO ^+-----------------------------------------------------^+
EXIT /B 1

:errorNotRegistered
ECHO ^+-----------------------------------------------------^+
ECHO ^| ERROR                                               ^|
ECHO ^+-----------------------------------------------------^+
ECHO ^|                                                     ^|
ECHO ^| Such driver does not appear to be registered.       ^|
ECHO ^| Was it registered under non-default name which      ^|
ECHO ^| then should be specified as the first parameter of  ^|
ECHO ^| this script?                                        ^|
ECHO ^|                                                     ^|
ECHO ^+-----------------------------------------------------^+
GOTO :eof
