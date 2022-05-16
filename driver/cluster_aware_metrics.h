/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __CLUSTERAWAREMETRICS_H__
#define __CLUSTERAWAREMETRICS_H__

#include <memory>

#include "cluster_aware_hit_metrics_holder.h"
#include "cluster_aware_time_metrics_holder.h"

class CLUSTER_AWARE_METRICS {
public:
	CLUSTER_AWARE_METRICS();
	~CLUSTER_AWARE_METRICS();
	void register_failure_detection_time(long long time_ms);
	void register_writer_failover_procedure_time(long long time_ms);
	void register_reader_failover_procedure_time(long long time_ms);
	void register_topology_query_time(long long time_ms);
	void register_failover_connects(bool is_hit);
	void register_invalid_initial_connection(bool is_hit);
	void register_use_cached_topology(bool is_hit);	

 	std::string report_metrics();

private:
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> failure_detection = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Failover Detection");
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> writer_failover_procedure = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Writer Failover Procedure");
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> reader_failover_procedure = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Reader Failover Procedure");
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> topology_query = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Topology Query");
	std::shared_ptr<CLUSTER_AWARE_HIT_METRICS_HOLDER> failover_connects = std::make_shared<CLUSTER_AWARE_HIT_METRICS_HOLDER>("Successful Failover Reconnects");
	std::shared_ptr<CLUSTER_AWARE_HIT_METRICS_HOLDER> invalid_initial_connection = std::make_shared<CLUSTER_AWARE_HIT_METRICS_HOLDER>("Invalid Initial Connection");
	std::shared_ptr<CLUSTER_AWARE_HIT_METRICS_HOLDER> use_cached_topology = std::make_shared<CLUSTER_AWARE_HIT_METRICS_HOLDER>("Used Cached Topology");
};

#endif /* __CLUSTERAWAREMETRICS_H__ */
