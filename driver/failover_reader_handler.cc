/**
  @file  failover_reader_handler.c
  @brief Failover functions.
*/

#include "failover.h"

//TODO: implement
FAILOVER_READER_HANDLER::FAILOVER_READER_HANDLER(TOPOLOGY_SERVICE* topology_service) {}
READER_FAILOVER_RESULT* FAILOVER_READER_HANDLER::failover(std::vector<HOST_INFO*>* hosts, HOST_INFO* currentHost) { return nullptr; }
READER_FAILOVER_RESULT* FAILOVER_READER_HANDLER::getReaderConnection(std::vector<HOST_INFO*> hosts) { return nullptr; }
FAILOVER_READER_HANDLER::~FAILOVER_READER_HANDLER() {}

MYSQL* FAILOVER_READER_HANDLER::get_reader_connection(CLUSTER_TOPOLOGY_INFO& topology_info, FAILOVER_CONNECTION_HANDLER* conn_handler, const std::function <bool()> is_canceled) {
    //TODO: implement
    return NULL;
}
