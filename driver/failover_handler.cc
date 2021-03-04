/**
  @file  failover_handler.c
  @brief Failover functions.
*/

#include "failover.h"


//TODO: implement
FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc) {
	this->dbc = dbc;
}
bool FAILOVER_HANDLER::triggerFailoverIfNeeded(const char* errorCode, const char* &newErrorCode) { return false; }
FAILOVER_HANDLER::~FAILOVER_HANDLER() { }
