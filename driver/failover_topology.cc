/**
  @file  failover_topology.c
  @brief Failover functions.
*/

#include "failover.h"

//TODO: implement
TOPOLOGY_SERVICE::TOPOLOGY_SERVICE() {}
void TOPOLOGY_SERVICE::setClusterId(const char* clusterId) {}
void TOPOLOGY_SERVICE::setClusterInstanceTemplate(HOST_INFO* hostTemplate) {}
std::vector<HOST_INFO*>* TOPOLOGY_SERVICE::getTopology() { return nullptr; }
std::vector<HOST_INFO*>* TOPOLOGY_SERVICE::getCachedTopology() { return nullptr; }
HOST_INFO* TOPOLOGY_SERVICE::getLastUsedReaderHost() { return nullptr; }
void TOPOLOGY_SERVICE::setLastUsedReaderHost(HOST_INFO* reader) {}
std::vector<std::string>* TOPOLOGY_SERVICE::getDownHosts() { return nullptr; }
void TOPOLOGY_SERVICE::addToDownHostList(HOST_INFO* downHost) {}
void TOPOLOGY_SERVICE::removeFromDownHostList(HOST_INFO* host) {}
void TOPOLOGY_SERVICE::setRefreshRate(int refreshRate) {}
void TOPOLOGY_SERVICE::clearAll() {}
void TOPOLOGY_SERVICE::clear() {}
TOPOLOGY_SERVICE::~TOPOLOGY_SERVICE() {}
