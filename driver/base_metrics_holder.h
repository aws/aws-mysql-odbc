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

#ifndef __BASEMETRICSHOLDER_H__
#define __BASEMETRICSHOLDER_H__

#include <climits>
#include <cmath>
#include <cstring>
#include <vector>

#include "mylog.h"

class BASE_METRICS_HOLDER {
public:
    BASE_METRICS_HOLDER();
    ~BASE_METRICS_HOLDER();

    virtual void register_query_execution_time(long long query_time_ms);
    virtual std::string report_metrics();

private:
    void create_initial_histogram(long long* breakpoints, long long lower_bound, long long upper_bound);
    void add_to_histogram(
        int* histogram_counts,
        long long* histogram_breakpoints,
        long long value,
        int number_of_times,
        long long current_lower_bound,
        long long current_upper_bound);
    void add_to_performance_histogram(long long value, int number_of_times);
    void check_and_create_performance_histogram();
    void repartition_histogram(
        int* hist_counts,
        long long* hist_breakpoints,
        long long current_lower_bound,
        long long current_upper_bound);
    void repartition_performance_histogram();
    
protected:
    const static int HISTOGRAM_BUCKETS = 20;
    long long longest_query_time_ms = 0;
    long number_of_queries_issued = 0;
    long* old_hist_breakpoints = nullptr;
    int* old_hist_counts = nullptr;
    long long shortest_query_time_ms = LONG_MAX;
    double total_query_time_ms = 0;
    long long* perf_metrics_hist_breakpoints = nullptr;
    int* perf_metrics_hist_counts = nullptr;
};

#endif /* __BASEMETRICSHOLDER_H__ */
