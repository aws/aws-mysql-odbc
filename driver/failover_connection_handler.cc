/**
  @file  failover_connection_handler.c
  @brief Failover connection functions.
*/

#include "driver.h"

FAILOVER_CONNECTION_HANDLER::FAILOVER_CONNECTION_HANDLER(DBC* dbc)
    : dbc{ dbc }, original_host{ nullptr }
{
}

FAILOVER_CONNECTION_HANDLER::~FAILOVER_CONNECTION_HANDLER()
{
}

MYSQL* FAILOVER_CONNECTION_HANDLER::connect(/*required info*/ HOST_INFO* hi) {
    // TODO implement
    return NULL;
}

void FAILOVER_CONNECTION_HANDLER::update_connection(MYSQL* new_conn) {
    // TODO implement
}

void FAILOVER_CONNECTION_HANDLER::release_connection(MYSQL* mysql) {
    mysql_close(mysql);
}

DBC* FAILOVER_CONNECTION_HANDLER::clone_dbc(DBC* source_dbc) {
    // TODO implement
    return NULL;
}

void FAILOVER_CONNECTION_HANDLER::release_dbc(DBC* dbc_clone) {
    // TODO implement
}
