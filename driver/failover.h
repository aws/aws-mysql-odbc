/**
  @file failover.h
  @brief Definitions needed for failover
*/

#ifndef __FAILOVER_H__
#define __FAILOVER_H__

//#include "sqltypes.h"
//#include "sql.h"

#include <stddef.h>
#include <vector>
#include <list>
#include <string>

struct DBC;

struct HOST_INFO {
	//TODO
};

class TOPOLOGY_SERVICE {
	private:
	//TODO

	public:
		TOPOLOGY_SERVICE();
		void setClusterId(const char* clusterId);
		void setClusterInstanceTemplate(HOST_INFO* hostTemplate);
		std::vector<HOST_INFO*>* getTopology();
		std::vector<HOST_INFO*>* getCachedTopology();
		HOST_INFO* getLastUsedReaderHost();
		void setLastUsedReaderHost(HOST_INFO* reader);
		std::vector<std::string>* getDownHosts();
		void addToDownHostList(HOST_INFO* downHost);
		void removeFromDownHostList(HOST_INFO* host);
		void setRefreshRate(int refreshRate);
		void clearAll();
		void clear();
		~TOPOLOGY_SERVICE();
};

struct READER_FAILOVER_RESULT {
	//TODO
};

class FAILOVER_READER_HANDLER {
	public:
		FAILOVER_READER_HANDLER();
		READER_FAILOVER_RESULT* failover(std::vector<HOST_INFO*> hosts, HOST_INFO* currentHost);
		READER_FAILOVER_RESULT* getReaderConnection(std::vector<HOST_INFO*> hosts);
		~FAILOVER_READER_HANDLER();
};

struct WRITER_FAILOVER_RESULT {
	//TODO
};

class FAILOVER_WRITER_HANDLER {
public:
	FAILOVER_WRITER_HANDLER();
	WRITER_FAILOVER_RESULT* failover(std::vector<HOST_INFO*> hosts);
	~FAILOVER_WRITER_HANDLER();
};

class FAILOVER_HANDLER {
	private:
		DBC* dbc = nullptr;
		TOPOLOGY_SERVICE* ts = nullptr;
		FAILOVER_READER_HANDLER* frh = nullptr;
		FAILOVER_WRITER_HANDLER* fwh = nullptr;
		std::vector<HOST_INFO*>* hosts = nullptr; //topology

	public:
		FAILOVER_HANDLER(DBC* dbc);
		bool triggerFailoverIfNeeded(const char* errorCode, const char* &newErrorCode);
		~FAILOVER_HANDLER();
};

#endif /* __FAILOVER_H__ */