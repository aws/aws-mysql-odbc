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

#include "base_metrics_holder.h"

BASE_METRICS_HOLDER::BASE_METRICS_HOLDER() {}

BASE_METRICS_HOLDER::~BASE_METRICS_HOLDER() {
    delete perf_metrics_hist_breakpoints;
    perf_metrics_hist_breakpoints = nullptr;

    delete perf_metrics_hist_counts;
    perf_metrics_hist_counts = nullptr;

    delete old_hist_breakpoints;
    old_hist_breakpoints = nullptr;

    delete old_hist_counts;
    old_hist_counts = nullptr;
}

void BASE_METRICS_HOLDER::register_query_execution_time(long long query_time_ms) {
    if (query_time_ms > longest_query_time_ms) {
        longest_query_time_ms = query_time_ms;

        repartition_performance_histogram();
    }

    add_to_performance_histogram(query_time_ms, 1);

    if (query_time_ms < shortest_query_time_ms) {
        shortest_query_time_ms = (query_time_ms == 0) ? 1 : query_time_ms;
    }

    number_of_queries_issued++;

    total_query_time_ms += query_time_ms;
}

std::string BASE_METRICS_HOLDER::report_metrics() {
    std::string log_message = "";

    log_message.append("** Performance Metrics Report **\n");
    log_message.append("\nLongest reported query: " + std::to_string(longest_query_time_ms) + " ms");
    log_message.append("\nShortest reported query: " + std::to_string(shortest_query_time_ms) + " ms");
    log_message.append("\nAverage query execution time: " + std::to_string(total_query_time_ms / number_of_queries_issued) + " ms");
    log_message.append("\nNumber of statements executed: " + number_of_queries_issued);

    if (perf_metrics_hist_breakpoints) {
        log_message.append("\n\n\tTiming Histogram:\n");
        int max_num_points = 20;
        int highest_count = INT_MIN;

        for (int i = 0; i < (HISTOGRAM_BUCKETS); i++) {
            if (perf_metrics_hist_counts[i] > highest_count) {
                highest_count = perf_metrics_hist_counts[i];
            }
        }

        if (highest_count == 0) {
            highest_count = 1; // avoid DIV/0
        }

        for (int i = 0; i < (HISTOGRAM_BUCKETS - 1); i++) {

            if (i == 0) {
                log_message.append("\n\tless than " + std::to_string(perf_metrics_hist_breakpoints[i + 1]) + " ms: \t" + std::to_string(perf_metrics_hist_counts[i]));
            } else {
                log_message.append("\n\tbetween " + std::to_string(perf_metrics_hist_breakpoints[i]) + " and " + std::to_string(perf_metrics_hist_breakpoints[i + 1]) + " ms: \t"
                        + std::to_string(perf_metrics_hist_counts[i]));
            }

            log_message.append("\t");

            int numPointsToGraph = (int) (max_num_points * ((double) perf_metrics_hist_counts[i] / highest_count));

            for (int j = 0; j < numPointsToGraph; j++) {
                log_message.append("*");
            }

            if (longest_query_time_ms < perf_metrics_hist_counts[i + 1]) {
                break;
            }
        }

        if (perf_metrics_hist_breakpoints[HISTOGRAM_BUCKETS - 2] < longest_query_time_ms) {
            log_message.append("\n\tbetween ");
            log_message.append(std::to_string(perf_metrics_hist_breakpoints[HISTOGRAM_BUCKETS - 2]));
            log_message.append(" and ");
            log_message.append(std::to_string(perf_metrics_hist_breakpoints[HISTOGRAM_BUCKETS - 1]));
            log_message.append(" ms: \t");
            log_message.append(std::to_string(perf_metrics_hist_counts[HISTOGRAM_BUCKETS - 1]));
        }
    }

    return log_message;
}

void BASE_METRICS_HOLDER::create_initial_histogram(long long* breakpoints, long long lower_bound, long long upper_bound) {
    long long bucket_size = (long long)((((double) upper_bound - (double) lower_bound) / HISTOGRAM_BUCKETS) * 1.25);

    if (bucket_size < 1) {
        bucket_size = 1;
    }

    for (int i = 0; i < HISTOGRAM_BUCKETS; i++) {
        breakpoints[i] = lower_bound;
        lower_bound += bucket_size;
    }
}

void BASE_METRICS_HOLDER::add_to_histogram(
    int* histogram_counts,
    long long* histogram_breakpoints,
    long long value,
    int number_of_times,
    long long current_lower_bound,
    long long current_upper_bound) {

    if (!histogram_counts) {
        create_initial_histogram(histogram_breakpoints, current_lower_bound, current_upper_bound);
    } else {
        for (int i = 1; i < HISTOGRAM_BUCKETS; i++) {
            if (histogram_breakpoints[i] >= value) {
                histogram_counts[i - 1] += number_of_times;
                break;
            }
        }
    }
}

void BASE_METRICS_HOLDER::add_to_performance_histogram(long long value, int number_of_times) {
    check_and_create_performance_histogram();

    add_to_histogram(perf_metrics_hist_counts, perf_metrics_hist_breakpoints, value, number_of_times,
        shortest_query_time_ms == LLONG_MAX ? 0 : shortest_query_time_ms, longest_query_time_ms);
}

void BASE_METRICS_HOLDER::check_and_create_performance_histogram() {
    if (!perf_metrics_hist_counts) {
        perf_metrics_hist_counts = new int[HISTOGRAM_BUCKETS]();
    }

    if (!perf_metrics_hist_breakpoints) {
        perf_metrics_hist_breakpoints = new long long[HISTOGRAM_BUCKETS]();
    }
}

void BASE_METRICS_HOLDER::repartition_histogram(
    int* hist_counts,
    long long* hist_breakpoints,
    long long current_lower_bound,
    long long current_upper_bound) {

    if (!old_hist_counts) {
        old_hist_counts = new int[HISTOGRAM_BUCKETS];
        old_hist_breakpoints = new long[HISTOGRAM_BUCKETS];
    }
    memcpy(old_hist_counts, hist_counts, HISTOGRAM_BUCKETS * sizeof(int));
    memcpy(old_hist_breakpoints, hist_breakpoints, HISTOGRAM_BUCKETS * sizeof(long));

    create_initial_histogram(hist_breakpoints, current_lower_bound, current_upper_bound);

    for (int i = 0; i < HISTOGRAM_BUCKETS; i++) {
        add_to_histogram(hist_counts, hist_breakpoints, old_hist_breakpoints[i], old_hist_counts[i], current_lower_bound, current_upper_bound);
    }
}

void BASE_METRICS_HOLDER::repartition_performance_histogram() {
    check_and_create_performance_histogram();

    repartition_histogram(perf_metrics_hist_counts, perf_metrics_hist_breakpoints,
        shortest_query_time_ms == LONG_MAX ? 0 : shortest_query_time_ms, longest_query_time_ms);
}
