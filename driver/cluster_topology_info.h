/*
 * AWS ODBC Driver for MySQL
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
 */

#ifndef __CLUSTERTOPOLOGYINFO_H__
#define __CLUSTERTOPOLOGYINFO_H__

#include "host_info.h"

#include <ctime>
#include <set>
#include <vector>

// This class holds topology information for one cluster.
// Cluster topology consists of an instance endpoint, a set of nodes in the cluster,
// the type of each node in the cluster, and the status of each node in the cluster.
class CLUSTER_TOPOLOGY_INFO {
public:
    CLUSTER_TOPOLOGY_INFO();
    CLUSTER_TOPOLOGY_INFO(const CLUSTER_TOPOLOGY_INFO& src_info); //copy constructor
    virtual ~CLUSTER_TOPOLOGY_INFO();

    void add_host(std::shared_ptr<HOST_INFO> host_info);
    size_t total_hosts();
    size_t num_readers(); // return number of readers in the cluster
    std::time_t time_last_updated();

    std::shared_ptr<HOST_INFO> get_writer();
    std::shared_ptr<HOST_INFO> get_next_reader();
    // TODO - Ponder if the get_reader below is needed. In general user of this should not need to deal with indexes.
    // One case that comes to mind, if we were to try to do a random shuffle of readers or hosts in general like JDBC driver
    // we could do random shuffle of host indices and call the get_reader for specific index in order we wanted.
    std::shared_ptr<HOST_INFO> get_reader(int i);
    std::vector<std::shared_ptr<HOST_INFO>> get_writers();
    std::vector<std::shared_ptr<HOST_INFO>> get_readers();
    bool is_multi_writer_cluster = false;

private:
    int current_reader = -1;
    std::time_t last_updated;
    std::set<std::string> down_hosts; // maybe not needed, HOST_INFO has is_host_down() method
    //std::vector<HOST_INFO*> hosts;
    std::shared_ptr<HOST_INFO> last_used_reader;  // TODO perhaps this overlaps with current_reader and is not needed

    // TODO - can we do without pointers -
    // perhaps ok for now, we are using copies CLUSTER_TOPOLOGY_INFO returned by get_topology and get_cached_topology from TOPOLOGY_SERVICE.
    // However, perhaps smart shared pointers could be used.
    std::vector<std::shared_ptr<HOST_INFO>> writers;
    std::vector<std::shared_ptr<HOST_INFO>> readers;

    std::shared_ptr<HOST_INFO> get_last_used_reader();
    void set_last_used_reader(std::shared_ptr<HOST_INFO> reader);
    void mark_host_down(std::shared_ptr<HOST_INFO> host);
    void mark_host_up(std::shared_ptr<HOST_INFO> host);
    std::set<std::string> get_down_hosts();
    void update_time();

    friend class TOPOLOGY_SERVICE;
};

#endif /* __CLUSTERTOPOLOGYINFO_H__ */
