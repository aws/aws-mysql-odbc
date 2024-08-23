# Modifications Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Copyright (c) 2007, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0, as
# published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation. The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# Without limiting anything contained in the foregoing, this file,
# which is part of Connector/ODBC, is also subject to the
# Universal FOSS Exception, version 1.0, a copy of which can be found at
# https://oss.oracle.com/licenses/universal-foss-exception.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

##########################################################################

# At least in CMake 2.8.12.2 the file "Dawin.cmake" defines
# CMAKE_C_CREATE_SHARED_LIBRARY without honoring
# CMAKE_SHARED_LINKER_FLAGS. I also failed to modify
# CMAKE_C_CREATE_SHARED_LIBRARY to include it. So as a
# work-around we add ODBC_LIB_DIR to the link directories.

IF(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  LINK_DIRECTORIES(${ODBC_LIB_DIR})
  IF(ODBCINST_LIB_DIR)
    LINK_DIRECTORIES(${ODBCINST_LIB_DIR})
  ENDIF()
ENDIF()

SET(DRIVER_INDEX 0)
SET(DRIVER_LOCATION2 "")
SET(CONNECTOR_DRIVER_TYPE2 "")
SET(CONNECTOR_DRIVER_TYPE_SHORT2 "")

WHILE(${DRIVER_INDEX} LESS ${DRIVERS_COUNT})

  LIST(GET IS_UNICODE_DRIVER ${DRIVER_INDEX} UNICODE)
  LIST(GET CONNECTOR_DRIVER_TYPES_SHORT ${DRIVER_INDEX} CONNECTOR_DRIVER_TYPE_SHORT)
  LIST(GET CONNECTOR_DRIVER_TYPE_LIST ${DRIVER_INDEX} DRIVER_TYPE)

  SET(DRIVER_NAME "awsmysqlodbc${CONNECTOR_DRIVER_TYPE_SHORT}")
  SET(DRIVER_NAME_STATIC "awsmysqlodbc${CONNECTOR_DRIVER_TYPE_SHORT}-static")

  SET(DRIVER_SRCS
    adfs_proxy.cc
    auth_util.cc
    aws_sdk_helper.cc
    base_metrics_holder.cc
    catalog.cc
    catalog_no_i_s.cc
    cluster_topology_info.cc
    cluster_aware_hit_metrics_holder.cc
    cluster_aware_metrics_container.cc
    cluster_aware_metrics.cc
    cluster_aware_time_metrics_holder.cc
    connect.cc
    connection_handler.cc
    connection_proxy.cc
    cursor.cc
    desc.cc
    dll.cc
    driver.cc
    efm_proxy.cc
    error.cc
    execute.cc
#    failover_handler.cc
#    failover_reader_handler.cc
#    failover_writer_handler.cc
    handle.cc
    host_info.cc
    iam_proxy.cc
    info.cc
    monitor.cc
    monitor_connection_context.cc
    monitor_thread_container.cc
    monitor_service.cc
    my_prepared_stmt.cc
    my_stmt.cc
    mylog.cc
    mysql_proxy.cc
    options.cc
    parse.cc
    prepare.cc
    query_parsing.cc
    results.cc
    secrets_manager_proxy.cc
    topology_service.cc
    transact.cc
    utility.cc)

  if(TELEMETRY)
      list(APPEND DRIVER_SRCS telemetry.cc)
  endif()

  IF(UNICODE)
    SET(DRIVER_SRCS ${DRIVER_SRCS} unicode.cc)
    SET(WIDECHARCALL "W")
  ELSE(UNICODE)
    SET(DRIVER_SRCS ${DRIVER_SRCS} ansi.cc)
    SET(WIDECHARCALL "")
  ENDIF(UNICODE)

  INCLUDE_DIRECTORIES(../util)

  IF(WIN32)

    # Headers added for convenience of VS users
    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/driver/driver.def.cmake ${CMAKE_SOURCE_DIR}/driver/driver${CONNECTOR_DRIVER_TYPE_SHORT}.def @ONLY)
    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/driver/driver.rc.cmake ${CMAKE_SOURCE_DIR}/driver/driver${CONNECTOR_DRIVER_TYPE_SHORT}.rc @ONLY)
    SET(DRIVER_SRCS ${DRIVER_SRCS} driver${CONNECTOR_DRIVER_TYPE_SHORT}.def driver${CONNECTOR_DRIVER_TYPE_SHORT}.rc
                                   adfs_proxy.h
                                   auth_util.h
                                   aws_sdk_helper.h
                                   base_metrics_holder.h
                                   catalog.h
                                   cluster_aware_hit_metrics_holder.h
                                   cluster_aware_metrics_container.h
                                   cluster_aware_metrics.h
                                   cluster_aware_time_metrics_holder.h
                                   cluster_topology_info.h
                                   connection_handler.h
                                   connection_proxy.h
                                   driver.h
                                   efm_proxy.h
                                   error.h
                                   failover.h
                                   host_info.h
                                   iam_proxy.h
                                   monitor.h
                                   monitor_connection_context.h
                                   monitor_thread_container.h
                                   monitor_service.h
                                   mylog.h
                                   mysql_proxy.h
                                   myutil.h
                                   parse.h
                                   query_parsing.h
                                   secrets_manager_proxy.h
                                   topology_service.h
                                   ../MYODBC_MYSQL.h ../MYODBC_CONF.h ../MYODBC_ODBC.h)
    if(TELEMETRY)
        list(APPEND DRIVER_SRCS telemetry.h)
    endif()
  ENDIF(WIN32)

  # Note: We build driver as a MODULE, because this is what it really is
  # -- it is not a shared library meant to be linked to the user code
  # but a dynamic module that will be loaded by ODBC manager. One
  # consequence of this is that on Windows import libraries will not
  # be generated nor installed.
  ADD_LIBRARY(${DRIVER_NAME_STATIC} STATIC ${DRIVER_SRCS} ${AWSSDK_LIB_DIR})
  ADD_LIBRARY(${DRIVER_NAME} SHARED ${DRIVER_SRCS} ${AWSSDK_LIB_DIR})

  ADD_COVERAGE(${DRIVER_NAME})

  IF(WIN32)
    INSTALL(TARGETS ${DRIVER_NAME}
      CONFIGURATIONS Release RelWithDebInfo
      DESTINATION ${LIB_SUBDIR})

    INSTALL(TARGETS ${DRIVER_NAME}
      CONFIGURATIONS Debug
      DESTINATION "${LIB_SUBDIR}/debug")

    INSTALL(TARGETS ${DRIVER_NAME_STATIC}
      CONFIGURATIONS Release RelWithDebInfo
      DESTINATION ${LIB_SUBDIR})

    INSTALL(TARGETS ${DRIVER_NAME_STATIC}
      CONFIGURATIONS Debug
      DESTINATION "${LIB_SUBDIR}/debug")
  ELSE()
    INSTALL(TARGETS ${DRIVER_NAME} DESTINATION ${LIB_SUBDIR})
    INSTALL(TARGETS ${DRIVER_NAME_STATIC} DESTINATION ${LIB_SUBDIR})
  ENDIF()

  IF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    SET_TARGET_PROPERTIES(${DRIVER_NAME} PROPERTIES
          LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}"
          PREFIX "")
    SET_TARGET_PROPERTIES(${DRIVER_NAME_STATIC} PROPERTIES
          LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}"
          PREFIX "")
  ELSE(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    SET_TARGET_PROPERTIES(${DRIVER_NAME} PROPERTIES
          LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}"
          PREFIX "") # -flto
    SET_TARGET_PROPERTIES(${DRIVER_NAME_STATIC} PROPERTIES
          LINK_FLAGS "${MYSQLODBCCONN_LINK_FLAGS_ENV} ${MYSQL_LINK_FLAGS}"
          PREFIX "") # -flto
  ENDIF()

  IF(UNICODE)
    SET_TARGET_PROPERTIES(${DRIVER_NAME} PROPERTIES
      COMPILE_DEFINITIONS MYODBC_UNICODEDRIVER)
    SET_TARGET_PROPERTIES(${DRIVER_NAME_STATIC} PROPERTIES
      COMPILE_DEFINITIONS MYODBC_UNICODEDRIVER)
  ENDIF(UNICODE)

  IF(WIN32)
    IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
      LINK_DIRECTORIES(${MYSQL_DIR}/lib/debug)
      LINK_DIRECTORIES(${MYSQL_DIR}/lib)
    ELSEIF(CMAKE_BUILD_TYPE STREQUAL "")
      LINK_DIRECTORIES(${MYSQL_DIR}/lib/opt)
      LINK_DIRECTORIES(${MYSQL_DIR}/lib)
    ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

    IF (WITH_NODEFAULTLIB)
      SET_TARGET_PROPERTIES(${DRIVER_NAME} PROPERTIES
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELEASE "/NODEFAULTLIB:${WITH_NODEFAULTLIB}")
      SET_TARGET_PROPERTIES(${DRIVER_NAME_STATIC} PROPERTIES
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELEASE "/NODEFAULTLIB:${WITH_NODEFAULTLIB}")
    ENDIF ()

    IF(MYSQL8)
      TARGET_LINK_LIBRARIES(${DRIVER_NAME} legacy_stdio_definitions.lib)
      TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC} legacy_stdio_definitions.lib)
    ENDIF(MYSQL8)

    TARGET_LINK_LIBRARIES(${DRIVER_NAME} myodbc-util
            ${MYSQL_CLIENT_LIBS} ws2_32 ${ODBCINSTLIB} ${SECURE32_LIB} Dnsapi)
    TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC} myodbc-util
            ${MYSQL_CLIENT_LIBS} ws2_32 ${ODBCINSTLIB} ${SECURE32_LIB} Dnsapi)

  ELSE(WIN32)

    IF(DEV_STUDIO_RUNTIME_DIR)
      FOREACH(__runtime_dir IN LISTS DEV_STUDIO_RUNTIME_DIR)
        SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -R${__runtime_dir}")
      ENDFOREACH()
    ENDIF()

    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ODBC_DRIVER_LINK_FLAGS}")
    TARGET_LINK_LIBRARIES(${DRIVER_NAME}
                          ${MYSQL_CLIENT_LIBS} ${CMAKE_THREAD_LIBS_INIT} m myodbc-util otel_api)
    TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC}
                          ${MYSQL_CLIENT_LIBS} ${CMAKE_THREAD_LIBS_INIT} m myodbc-util otel_api)

    IF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
      TARGET_LINK_LIBRARIES(${DRIVER_NAME} resolv)
      TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC} resolv)
    ENDIF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")


    INCLUDE_DIRECTORIES(${DL_INCLUDES})

  ENDIF(WIN32)

  IF (MYSQL_CXX_LINKAGE)
    SET_TARGET_PROPERTIES(${DRIVER_NAME} PROPERTIES
          LINKER_LANGUAGE CXX
          COMPILE_FLAGS "${MYSQLODBCCONN_COMPILE_FLAGS_ENV} ${MYSQL_CXXFLAGS}")
    SET_TARGET_PROPERTIES(${DRIVER_NAME_STATIC} PROPERTIES
          LINKER_LANGUAGE CXX
          COMPILE_FLAGS "${MYSQLODBCCONN_COMPILE_FLAGS_ENV} ${MYSQL_CXXFLAGS}")
  ENDIF (MYSQL_CXX_LINKAGE)

  set_property(TARGET ${DRIVER_NAME} PROPERTY BUILD_WITH_INSTALL_RPATH ON)
  set_property(TARGET ${DRIVER_NAME_STATIC} PROPERTY BUILD_WITH_INSTALL_RPATH ON)

  if(APPLE)
    set_property(TARGET ${DRIVER_NAME} APPEND PROPERTY INSTALL_RPATH "@loader_path")
    set_property(TARGET ${DRIVER_NAME} APPEND PROPERTY INSTALL_RPATH "@loader_path/private")
    set_property(TARGET ${DRIVER_NAME} APPEND PROPERTY INSTALL_RPATH "/usr/local/iODBC/lib")
    set_property(TARGET ${DRIVER_NAME} APPEND PROPERTY INSTALL_RPATH "/usr/local/var/homebrew/linked/libiodbc/lib")
    mangle_osx_rpaths(${DRIVER_NAME})
  elseif(NOT WIN32)
    set_property(TARGET ${DRIVER_NAME} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    set_property(TARGET ${DRIVER_NAME} APPEND PROPERTY INSTALL_RPATH "$ORIGIN/private")
  endif()

  # FIXME: RPATH not set anywhere...
  #MESSAGE("Setting RPATH for ${DRIVER_NAME}")

  MATH(EXPR DRIVER_INDEX "${DRIVER_INDEX} + 1")

  #------------AWS SDK------------------
  LIST(APPEND SERVICE_LIST rds secretsmanager)

  MESSAGE(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
  IF(MSVC)
    MESSAGE(STATUS "Copying AWS SDK libraries to ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE}")
    AWSSDK_CPY_DYN_LIBS(SERVICE_LIST "" ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
  ENDIF(MSVC)

  TARGET_INCLUDE_DIRECTORIES(${DRIVER_NAME} PUBLIC ${AWSSDK_INCLUDE_DIR})
  TARGET_LINK_LIBRARIES(${DRIVER_NAME} ${AWSSDK_LINK_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(${DRIVER_NAME_STATIC} PUBLIC ${AWSSDK_INCLUDE_DIR})
  TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC} ${AWSSDK_LINK_LIBRARIES})
  #------------------------------

  # TODO - Need to copy the DLL to the release folder / lib
  #        CONFIGURE_FILE?
  # TODO - Need proper pathing / remove hardcoding

  # TODO - Test Exported IAM Auth
  TARGET_INCLUDE_DIRECTORIES(${DRIVER_NAME} PRIVATE "C:/Users/yuenhcol/Code/aws-iam-proxy")
  TARGET_LINK_LIBRARIES(${DRIVER_NAME} "C:/Users/yuenhcol/Code/aws-iam-proxy/build/Release/aws-odbc-iam-proxy.lib")
  TARGET_INCLUDE_DIRECTORIES(${DRIVER_NAME_STATIC} PRIVATE "C:/Users/yuenhcol/Code/aws-iam-proxy")
  TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC} "C:/Users/yuenhcol/Code/aws-iam-proxy/build/Release/aws-odbc-iam-proxy.lib")

  # TODO - Test Exported Failover
  TARGET_INCLUDE_DIRECTORIES(${DRIVER_NAME} PRIVATE "C:/Users/yuenhcol/Documents/Code/aws-odbc-failover")
  TARGET_LINK_LIBRARIES(${DRIVER_NAME} "C:/Users/yuenhcol/Documents/Code/aws-odbc-failover/build/Release/aws-odbc-failover-export.lib")
  TARGET_INCLUDE_DIRECTORIES(${DRIVER_NAME_STATIC} PRIVATE "C:/Users/yuenhcol/Documents/Code/aws-odbc-failover")
  TARGET_LINK_LIBRARIES(${DRIVER_NAME_STATIC} "C:/Users/yuenhcol/Documents/Code/aws-odbc-failover/build/Release/aws-odbc-failover-export.lib")


ENDWHILE(${DRIVER_INDEX} LESS ${DRIVERS_COUNT})
