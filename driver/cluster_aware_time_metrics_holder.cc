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

#include "cluster_aware_time_metrics_holder.h"

CLUSTER_AWARE_TIME_METRICS_HOLDER::CLUSTER_AWARE_TIME_METRICS_HOLDER(std::string metric_name):metric_name{metric_name} {}

CLUSTER_AWARE_TIME_METRICS_HOLDER::~CLUSTER_AWARE_TIME_METRICS_HOLDER() {}

std::string CLUSTER_AWARE_TIME_METRICS_HOLDER::report_metrics() {
    std::string log_message = "";

    log_message.append("\n** Performance Metrics Report for '").append(metric_name).append("' **\n");
    if (number_of_queries_issued > 0) {
      log_message.append("\nLongest reported time: ").append(std::to_string(longest_query_time_ms)).append(" ms");
      log_message.append("\nShortest reported time: ").append(std::to_string(shortest_query_time_ms)).append(" ms");
      double avg_time = total_query_time_ms / number_of_queries_issued;
      log_message.append("\nAverage query execution time: ").append(std::to_string(avg_time)).append(" ms");
    }
    log_message.append("\nNumber of reports: ").append(std::to_string(number_of_queries_issued));

    if (number_of_queries_issued > 0 && perf_metrics_hist_breakpoints) {
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
          log_message.append("\n\tless than ")
                  .append(std::to_string(perf_metrics_hist_breakpoints[i + 1]))
                  .append(" ms: \t")
                  .append(std::to_string(perf_metrics_hist_counts[i]));
        } else {
          log_message.append("\n\tbetween ")
                  .append(std::to_string(perf_metrics_hist_breakpoints[i]))
                  .append(" and ")
                  .append(std::to_string(perf_metrics_hist_breakpoints[i + 1]))
                  .append(" ms: \t")
                  .append(std::to_string(perf_metrics_hist_counts[i]));
        }

        log_message.append("\t");

        int numPointsToGraph =
            (int) (max_num_points * ((double) perf_metrics_hist_counts[i] / highest_count));

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

void CLUSTER_AWARE_TIME_METRICS_HOLDER::register_query_execution_time(long query_time_ms) {
	BASE_METRICS_HOLDER::register_query_execution_time(query_time_ms);
}

void CLUSTER_AWARE_TIME_METRICS_HOLDER::debug() {
    printf("Metric Name: %s\n", metric_name.c_str());
    printf("\tExecute Times: %d\n", number_of_queries_issued);
}
