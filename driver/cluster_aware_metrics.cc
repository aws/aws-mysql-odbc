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

#include "cluster_aware_metrics.h"

CLUSTER_AWARE_METRICS::CLUSTER_AWARE_METRICS() {}

CLUSTER_AWARE_METRICS::~CLUSTER_AWARE_METRICS() {}

void CLUSTER_AWARE_METRICS::register_failure_detection_time(long long time_ms) {
	failure_detection->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_writer_failover_procedure_time(long long time_ms) {
	writer_failover_procedure->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_reader_failover_procedure_time(long long time_ms) {
	reader_failover_procedure->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_topology_query_time(long long time_ms) {
	topology_query->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_failover_connects(bool is_hit) {
	failover_connects->register_metrics(is_hit);
}

void CLUSTER_AWARE_METRICS::register_invalid_initial_connection(bool is_hit) {
	invalid_initial_connection->register_metrics(is_hit);
}

void CLUSTER_AWARE_METRICS::register_use_cached_topology(bool is_hit) {
	use_cached_topology->register_metrics(is_hit);
}

std::string CLUSTER_AWARE_METRICS::report_metrics() {
	std::string log_message = "";
	log_message.append(failover_connects->report_metrics());
	log_message.append(failure_detection->report_metrics());
	log_message.append(writer_failover_procedure->report_metrics());
	log_message.append(reader_failover_procedure->report_metrics());
	log_message.append(topology_query->report_metrics());
	log_message.append(use_cached_topology->report_metrics());
	log_message.append(invalid_initial_connection->report_metrics());
	return log_message;
}
