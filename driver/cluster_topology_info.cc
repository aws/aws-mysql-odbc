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

#include "cluster_topology_info.h"

#include <stdexcept>

/**
  Initialize and return random number.

  Returns random number.
 */
int get_random_number() {
    std::srand((unsigned int)time(nullptr));
    return rand();
}

CLUSTER_TOPOLOGY_INFO::CLUSTER_TOPOLOGY_INFO() {
    update_time();
}

// copy constructor
CLUSTER_TOPOLOGY_INFO::CLUSTER_TOPOLOGY_INFO(const CLUSTER_TOPOLOGY_INFO& src_info)
    : current_reader{src_info.current_reader},
      last_updated{src_info.last_updated},
      last_used_reader{src_info.last_used_reader},
      is_multi_writer_cluster{src_info.is_multi_writer_cluster} {
    for (auto host_info_source : src_info.writers) {
        writers.push_back(std::make_shared<HOST_INFO>(*host_info_source)); //default copy
    }
    for (auto host_info_source : src_info.readers) {
        readers.push_back(std::make_shared<HOST_INFO>(*host_info_source)); //default copy
    }
}

CLUSTER_TOPOLOGY_INFO::~CLUSTER_TOPOLOGY_INFO() {
    for (auto p : writers) {
        p.reset();
    }
    writers.clear();

    for (auto p : readers) {
        p.reset();
    }
    readers.clear();
}

void CLUSTER_TOPOLOGY_INFO::add_host(std::shared_ptr<HOST_INFO> host_info) {
    host_info->is_host_writer() ? writers.push_back(host_info) : readers.push_back(host_info);
    update_time();
}

size_t CLUSTER_TOPOLOGY_INFO::total_hosts() {
    return writers.size() + readers.size();
}

size_t CLUSTER_TOPOLOGY_INFO::num_readers() {
    return readers.size();
}

std::time_t CLUSTER_TOPOLOGY_INFO::time_last_updated() {
    return last_updated;
}

// TODO harmonize time function across objects so the times are comparable
void CLUSTER_TOPOLOGY_INFO::update_time() {
    last_updated = time(0);
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_writer() {
    if (writers.size() <= 0) {
        throw std::runtime_error("No writer available");
    }

    return writers[0];
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_next_reader() {
    size_t num_readers = readers.size();
    if (num_readers <= 0) {
        throw std::runtime_error("No reader available");
    }

    if (current_reader == -1) { // initialize for the first time
        current_reader = get_random_number() % num_readers;
    }
    else if (current_reader >= num_readers) {
        // adjust current reader in case topology was refreshed.
        current_reader = (current_reader) % num_readers;
    }
    else {
        current_reader = (current_reader + 1) % num_readers;
    }

    return readers[current_reader];
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_reader(int i) {
    if (i < 0 || i >= readers.size()) {
        throw std::runtime_error("No reader available at index " + i);
    }

    return readers[i];
}

std::vector<std::shared_ptr<HOST_INFO>> CLUSTER_TOPOLOGY_INFO::get_readers() {
    return readers;
}

std::vector<std::shared_ptr<HOST_INFO>> CLUSTER_TOPOLOGY_INFO::get_writers() {
    return writers;
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_last_used_reader() {
    return last_used_reader;
}

void CLUSTER_TOPOLOGY_INFO::set_last_used_reader(std::shared_ptr<HOST_INFO> reader) {
    last_used_reader = reader;
}

void CLUSTER_TOPOLOGY_INFO::mark_host_down(std::shared_ptr<HOST_INFO> host) {
    host->set_host_state(DOWN);
    down_hosts.insert(host->get_host_port_pair());
}

void CLUSTER_TOPOLOGY_INFO::mark_host_up(std::shared_ptr<HOST_INFO> host) {
    host->set_host_state(UP);
    down_hosts.erase(host->get_host_port_pair());
}

std::set<std::string> CLUSTER_TOPOLOGY_INFO::get_down_hosts() {
    return down_hosts;
}
