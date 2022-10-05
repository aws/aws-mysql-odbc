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

#include "base_failover_integration_test.cc"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <thread>

#include <OpenXLSX.hpp>

#define SOCKET_TIMEOUT_TEST_ID 1
#define EFM_FAILOVER_TEST_ID 2
#define EFM_DETECTION_TEST_ID 3

struct BASE_PERFORMANCE_DATA {
  int network_outage_delay;
  int min_failover_time, max_failover_time, avg_failover_time;
  virtual void write_header(std::ofstream& data_stream) {}
  virtual void write_data_row(std::ofstream& data_stream) {}
  virtual void write_header_xlsx(OpenXLSX::XLWorksheet worksheet, int row) {}
  virtual void write_data_row_xlsx(OpenXLSX::XLWorksheet worksheet, int row) {}
};

struct SOCKET_PERFORMANCE_DATA : BASE_PERFORMANCE_DATA {
  int failover_timeout, connect_timeout, network_timeout;

  void write_header(std::ofstream& data_stream) override {
    data_stream << "Outage Delay (ms), Failover Timeout (ms), Connect Timeout (s), Network Timeout (s), Min Failover Time (ms), Max Failover Time (ms), Avg Failover Time (ms)\n";
  }

  void write_header_xlsx(const OpenXLSX::XLWorksheet worksheet, int row) {
    worksheet.cell(row, 1).value() = "Outage Delay (ms)";
    worksheet.cell(row, 2).value() = "Failover Timeout (ms)";
    worksheet.cell(row, 3).value() = "Connect Timeout (s)";
    worksheet.cell(row, 4).value() = "Network Timeout (s)";
    worksheet.cell(row, 5).value() = "Min Failover Time (ms)";
    worksheet.cell(row, 6).value() = "Max Failover Time (ms)";
    worksheet.cell(row, 7).value() = "Avg Failover Time (ms)";
  }

  void write_data_row(std::ofstream& data_stream) override {
    char data_row[4096];
    sprintf(data_row, "%d, %d, %d, %d, %d, %d, %d\n",
      this->network_outage_delay,
      this->failover_timeout,
      this->connect_timeout,
      this->network_timeout,
      this->min_failover_time,
      this->max_failover_time,
      this->avg_failover_time
    );
    data_stream << data_row;
  }

  void write_data_row_xlsx(const OpenXLSX::XLWorksheet worksheet, int row) override {
    worksheet.cell(row, 1).value() = this->network_outage_delay;
    worksheet.cell(row, 2).value() = this->failover_timeout;
    worksheet.cell(row, 3).value() = this->connect_timeout;
    worksheet.cell(row, 4).value() = this->network_timeout;
    worksheet.cell(row, 5).value() = this->min_failover_time;
    worksheet.cell(row, 6).value() = this->max_failover_time;
    worksheet.cell(row, 7).value() = this->avg_failover_time;
  }
};

struct EFM_PERFORMANCE_DATA : BASE_PERFORMANCE_DATA {
  int detection_time, detection_interval, detection_count;

  void write_header(std::ofstream& data_stream) override {
    data_stream << "Outage Delay (ms), Detection Time (?), Detection Interval (?), Detection Count, Min Failover Time (ms), Max Failover Time (ms), Avg Failover Time (ms)\n";
  }

  void write_header_xlsx(const OpenXLSX::XLWorksheet worksheet, int row) {
    worksheet.cell(row, 1).value() = "Outage Delay (ms)";
    worksheet.cell(row, 2).value() = "Detection Time (?)";
    worksheet.cell(row, 3).value() = "Detection Interval (?)";
    worksheet.cell(row, 4).value() = "Detection Count";
    worksheet.cell(row, 5).value() = "Min Failover Time (ms)";
    worksheet.cell(row, 6).value() = "Max Failover Time (ms)";
    worksheet.cell(row, 7).value() = "Avg Failover Time (ms)";
  }

  void write_data_row(std::ofstream& data_stream) override {
    char data_row[4096];
    sprintf(data_row, "%d, %d, %d, %d, %d, %d, %d\n",
      this->network_outage_delay,
      this->detection_time,
      this->detection_interval,
      this->detection_count,
      this->min_failover_time,
      this->max_failover_time,
      this->avg_failover_time
    );
    data_stream << data_row;
  }

  void write_data_row_xlsx(const OpenXLSX::XLWorksheet worksheet, int row) override {
    worksheet.cell(row, 1).value() = this->network_outage_delay;
    worksheet.cell(row, 2).value() = this->detection_time;
    worksheet.cell(row, 3).value() = this->detection_interval;
    worksheet.cell(row, 4).value() = this->detection_count;
    worksheet.cell(row, 5).value() = this->min_failover_time;
    worksheet.cell(row, 6).value() = this->max_failover_time;
    worksheet.cell(row, 7).value() = this->avg_failover_time;
  }
};

static std::vector<SOCKET_PERFORMANCE_DATA> socket_failover_data;
static std::vector<EFM_PERFORMANCE_DATA> efm_failover_data;
static std::vector<EFM_PERFORMANCE_DATA> efm_detection_data;

char* get_time() {
    time_t now = time(nullptr);
    //char time[256];
    //strftime(time, sizeof(time), "%Y/%m/%d %X ", localtime(&now));
    char* time = asctime(gmtime(&now));
    time[strlen(time) - 1] = '\0';    // Remove \n
    return time;
}

//#define print_log(f_, ...) printf("%s ", get_time()), printf((f_), ##__VA_ARGS__), printf("\n")

static FILE* log_file = nullptr;

#define DRIVER_LOG_FILE "perf_test.log"

void print_log(const char* fmt, ...) {
    if (!log_file) {
        printf("Initializing log_file\n");
        log_file = fopen(DRIVER_LOG_FILE, "a+");
        if (log_file) {
            printf("log_file initialized\n");
            fprintf(log_file, "\n\n-- Perf Test logging\n");
        }
        else {
            printf("Failed to initialize log_file\n");
        }
    }
    
    if (log_file && fmt) {
        time_t now = time(nullptr);
        char time_buf[256];
        strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %X ", localtime(&now));

        va_list args1;
        va_start(args1, fmt);
        va_list args2;
        va_copy(args2, args1);
        std::vector<char> buf(1 + vsnprintf(nullptr, 0, fmt, args1));
        va_end(args1);
        vsnprintf(buf.data(), buf.size(), fmt, args2);
        va_end(args2);

        printf("%s - %s\n", time_buf, buf.data());
        fprintf(log_file, "%s - %s\n", time_buf, buf.data());
        fflush(log_file);
    }
}

void print_data(EFM_PERFORMANCE_DATA data) {
  print_log("[print_data] Outage delay = %d", data.network_outage_delay);
  print_log("[print_data] Detection time = %d", data.detection_time);
  print_log("[print_data] Detection interval = %d", data.detection_interval);
  print_log("[print_data] Detection count = %d", data.detection_count);
  print_log("[print_data] Min Failover time = %d", data.min_failover_time);
  print_log("[print_data] Max Failover time = %d", data.max_failover_time);
  print_log("[print_data] Avf Failover time = %d", data.avg_failover_time);
}

class FailoverPerformanceTest :
  public ::testing::WithParamInterface<std::tuple<int, int, int, int, int>>,
  public BaseFailoverIntegrationTest {
protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");
  Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY),
                                                                    Aws::String(SECRET_ACCESS_KEY),
                                                                    Aws::String(SESSION_TOKEN));
  Aws::Client::ClientConfiguration client_config;
  Aws::RDS::RDSClient rds_client;
  SQLHENV env = nullptr;
  SQLHDBC dbc = nullptr;

  SQLCHAR* LONG_QUERY = AS_SQLCHAR("SELECT SLEEP(600)"); // 600s -> 10m
  const size_t NB_OF_RUNS = 3;
  static constexpr char* OUTPUT_FILE_PATH = "./build/reports/";

  static void SetUpTestSuite() {
    Aws::InitAPI(options);
  }

  static void TearDownTestSuite() {
    Aws::ShutdownAPI(options);

    // Save results to spreadsheet
    write_metrics_to_xlsx("failover_performance.xlsx", socket_failover_data);

    // Save results from EFM performance tests
    write_metrics_to_xlsx("efm_performance.xlsx", efm_failover_data);

    // Save results from EFM without performance tests
    write_metrics_to_xlsx("efm_detection_performance.xlsx", efm_detection_data);
  }

  void SetUp() override {
    print_log("[SetUp()] Calling SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env)");
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    
    print_log("[SetUp()] Calling SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0)");
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    
    print_log("[SetUp()] Calling SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc)");
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    
    client_config.region = "us-east-2";
    rds_client = Aws::RDS::RDSClient(credentials, client_config);

    cluster_instances = retrieve_topology_via_SDK(rds_client, cluster_id);
    writer_id = get_writer_id(cluster_instances);
  }

  void TearDown() override {
    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
  }

  // Only run if passed in parameter: data, is a type of BASE_PERFORMANCE_DATA
  template<typename DERIVED_PERFORMANCE_DATA, typename Enable = std::enable_if<std::is_base_of<BASE_PERFORMANCE_DATA, DERIVED_PERFORMANCE_DATA>::value>>
  bool measure_performance(const std::string& conn_str, const int sleep_delay, DERIVED_PERFORMANCE_DATA& data) {
    std::atomic<std::chrono::steady_clock::time_point> downtime(std::chrono::steady_clock::now());
    std::vector<int> recorded_metrics;

    print_log("[measure_performance] NB_OF_RUNS = %zd", NB_OF_RUNS);

    for (size_t i = 0; i < NB_OF_RUNS; i++) {
      // Ensure all proxies are up
      for (const auto& x : proxy_map) {
        enable_connectivity(x.second);
      }

      EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(conn_str.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));
      print_log("[measure_performance] SQLDriverConnect()");
      SQLHSTMT handle;
      EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
      print_log("[measure_performance] SQLAllocHandle()");

      // Start thread for shutdown
      auto network_shutdown_function = [&](const std::string& instance_id, const int sleep_ms, std::atomic<std::chrono::steady_clock::time_point>& downtime_detection) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        disable_instance(instance_id);
        downtime_detection.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
      };
      std::thread network_shutdown_thread(network_shutdown_function, std::cref(writer_id), std::cref(sleep_delay), std::ref(downtime));

      // Execute long query and wait for error / failover.
      print_log("[measure_performance] executing query = '%s'", LONG_QUERY);
      if (SQL_ERROR == SQLExecDirect(handle, LONG_QUERY, SQL_NTS)) {
        print_log("[measure_performance] query failed");
          
        int failover_time = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - downtime.load(std::memory_order_relaxed)).count());
        recorded_metrics.push_back(failover_time);
      }

      // Ensure thread stops
      if (network_shutdown_thread.joinable()) {
        network_shutdown_thread.join();
      }

      EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
      print_log("[measure_performance] SQLFreeHandle()");
      SQLRETURN rc = SQLDisconnect(dbc);
      print_log("[measure_performance] SQLDisconnect() returned %d", rc);
      EXPECT_EQ(SQL_SUCCESS, rc);
    }

    print_log("[measure_performance] Outside loop");

    // Metrics Statistics
    const size_t recorded_metrics_size = recorded_metrics.size();
    if (recorded_metrics_size < 1) {
      return false;
    }
    int min = recorded_metrics[0];
    int max = recorded_metrics[0];
    int sum = recorded_metrics[0];
    for (size_t i = 1; i < recorded_metrics_size; i++) {
      min = std::min(min, recorded_metrics[i]);
      max = std::max(max, recorded_metrics[i]);
      sum += recorded_metrics[i];
    }
    int avg = static_cast<int>(sum / recorded_metrics_size);

    data.min_failover_time = min;
    data.max_failover_time = max;
    data.avg_failover_time = avg;
    return true;
  }

  // Only run if passed in parameter: data_list contents, are a type of BASE_PERFORMANCE_DATA
  template<typename DERIVED_PERFORMANCE_DATA, typename Enable = std::enable_if<std::is_base_of<BASE_PERFORMANCE_DATA, DERIVED_PERFORMANCE_DATA>::value>>
  static void write_metrics_to_file(const char* file_name, const std::vector<DERIVED_PERFORMANCE_DATA>& data_list) {
    if (data_list.size() < 2) {
      return;
    }

    std::ofstream data_file(std::string(OUTPUT_FILE_PATH) + file_name);

    DERIVED_PERFORMANCE_DATA data = data_list[0];
    data.write_header(data_file);
    for (size_t i = 1; i < data_list.size(); i++) {
      data = data_list[i];
      data.write_data_row(data_file);
    }

    data_file.close();
  }

  template<typename DERIVED_PERFORMANCE_DATA, typename Enable = std::enable_if<std::is_base_of<BASE_PERFORMANCE_DATA, DERIVED_PERFORMANCE_DATA>::value>>
  static void write_metrics_to_xlsx(const char* file_name, const std::vector<DERIVED_PERFORMANCE_DATA>& data_list) {
    if (data_list.size() < 2) {
      return;
    }

    OpenXLSX::XLDocument doc;
    doc.create(file_name);

    std::string XLSX_WORKSHEET_NAME = "failover_performance";
    doc.workbook().addWorksheet(XLSX_WORKSHEET_NAME);
    OpenXLSX::XLWorksheet worksheet = doc.workbook().worksheet(XLSX_WORKSHEET_NAME);

    DERIVED_PERFORMANCE_DATA data = data_list[0];

    int row = 1;
    data.write_header_xlsx(worksheet, row);

    for (size_t i = 1; i < data_list.size(); i++) {
      data = data_list[i];
      row++;
      data.write_data_row_xlsx(worksheet, row);
    }

    doc.save();
    doc.close();
  }
};

TEST_P(FailoverPerformanceTest, test_measure_failover) {
  print_log("[test_measure_failover] Starting Test");

  const int test_type = std::get<0>(GetParam());
  const int sleep_delay = std::get<1>(GetParam());
  const std::string server = get_proxied_endpoint(writer_id);
  
  print_log("[test_measure_failover] Starting builder");
  builder.withDSN(dsn).withServer(server).withPort(MYSQL_PROXY_PORT)
    .withDatabase(db).withUID(user).withPWD(pwd).withHostPattern(PROXIED_CLUSTER_TEMPLATE)
    .withAllowReaderConnections(true);
  print_log("[test_measure_failover] Finished builder");

  switch (test_type) {
    case SOCKET_TIMEOUT_TEST_ID: {
      const int failover_timeout = std::get<2>(GetParam());
      const int connect_timeout = std::get<3>(GetParam());
      const int network_timeout = std::get<4>(GetParam());

      const std::string conn_str = builder.withEnableFailureDetection(false)
                                          .withEnableClusterFailover(true)
                                          .withFailoverTimeout(failover_timeout)
                                          .withConnectTimeout(connect_timeout)
                                          .withNetworkTimeout(network_timeout)
                                          .withLogQuery(true).build();

      SOCKET_PERFORMANCE_DATA data;
      data.network_outage_delay = sleep_delay;
      data.failover_timeout = failover_timeout;
      data.connect_timeout = connect_timeout;
      data.network_timeout = network_timeout;

      if (measure_performance(conn_str, sleep_delay, data)) {
        socket_failover_data.push_back(data);
      }
      break;
    }

    case EFM_FAILOVER_TEST_ID: {
      const int detection_time = std::get<2>(GetParam());
      print_log("[test_measure_failover] detection_time = %d", detection_time);

      const int detection_interval = std::get<3>(GetParam());
      print_log("[test_measure_failover] detection_interval = %d", detection_interval);

      const int detection_count = std::get<4>(GetParam());
      print_log("[test_measure_failover] detection_count = %d", detection_count);

      std::string conn_str = builder.withEnableFailureDetection(true)
                                          .withEnableClusterFailover(true)
                                          .withFailureDetectionTime(detection_time)
                                          .withFailureDetectionInterval(detection_interval)
                                          .withFailureDetectionCount(detection_count).build();

      print_log("[test_measure_failover] conn_str = '%s'", conn_str);

      EFM_PERFORMANCE_DATA data;
      data.network_outage_delay = sleep_delay;
      data.detection_time = detection_time;
      data.detection_interval = detection_interval;
      data.detection_count = detection_count;

      if (measure_performance(conn_str, sleep_delay, data)) {
        print_log("[test_measure_failover] measure_performance returned true");
        print_data(data);
        efm_failover_data.push_back(data);
      }
      else {
          print_log("[test_measure_failover] measure_performance returned false");
      }
      break;
    }

    case EFM_DETECTION_TEST_ID: {
      const int detection_time = std::get<2>(GetParam());
      print_log("[test_measure_failover] detection_time = %d", detection_time);

      const int detection_interval = std::get<3>(GetParam());
      print_log("[test_measure_failover] detection_interval = %d", detection_interval);

      const int detection_count = std::get<4>(GetParam());
      print_log("[test_measure_failover] detection_count = %d", detection_count);

      std::string conn_str = builder.withEnableFailureDetection(true)
                                          .withEnableClusterFailover(false)
                                          .withFailureDetectionTime(detection_time)
                                          .withFailureDetectionInterval(detection_interval)
                                          .withFailureDetectionCount(detection_count).build();

      print_log("[test_measure_failover] conn_str = '%s'", conn_str);

      EFM_PERFORMANCE_DATA data;
      data.network_outage_delay = sleep_delay;
      data.detection_time = detection_time;
      data.detection_interval = detection_interval;
      data.detection_count = detection_count;

      if (measure_performance(conn_str, sleep_delay, data)) {
        print_log("[test_measure_failover] measure_performance returned true");
        print_data(data);
        efm_detection_data.push_back(data);
      }
      else {
          print_log("[test_measure_failover] measure_performance returned false");
      }
      break;
    }

    default:
      throw std::runtime_error("No Such Parameterized Test Case");
    }
}

// INSTANTIATE_TEST_CASE_P(
//  SocketTimeoutTest,
//  FailoverPerformanceTest,
//  // Test Type, Sleep_Delay_ms, Failover_Timeout_ms, Connection_Timeout_s, Network_Timeout_s
//  ::testing::Values(
//    std::make_tuple(SOCKET_TIMEOUT_TEST_ID, 5000,  30000, 30, 30),
//    std::make_tuple(SOCKET_TIMEOUT_TEST_ID, 10000, 30000, 30, 30),
//    std::make_tuple(SOCKET_TIMEOUT_TEST_ID, 15000, 30000, 30, 30),
//    std::make_tuple(SOCKET_TIMEOUT_TEST_ID, 20000, 30000, 30, 30),
//    std::make_tuple(SOCKET_TIMEOUT_TEST_ID, 25000, 30000, 30, 30),
//    std::make_tuple(SOCKET_TIMEOUT_TEST_ID, 30000, 30000, 30, 30)
//  )
// );

// INSTANTIATE_TEST_CASE_P(
//   EFMTimeoutTest,
//   FailoverPerformanceTest,
//   // Test Type, Sleep Delay, detection grace time, detection interval, detection count
//   ::testing::Values(
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 5000,  30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 10000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 15000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 20000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 25000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 30000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 35000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 40000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 45000, 30000, 5000, 3),
//     std::make_tuple(EFM_FAILOVER_TEST_ID, 50000, 30000, 5000, 3)
//   )
// );

INSTANTIATE_TEST_CASE_P(
  EFMDetectionTimeoutTest,
  FailoverPerformanceTest,
  // Test Type, Sleep Delay, detection grace time, detection interval, detection count
  ::testing::Values(
    std::make_tuple(EFM_DETECTION_TEST_ID, 5000,  30000, 5000, 3),
    std::make_tuple(EFM_DETECTION_TEST_ID, 10000, 30000, 5000, 3),
    std::make_tuple(EFM_DETECTION_TEST_ID, 15000, 30000, 5000, 3)
    // std::make_tuple(EFM_DETECTION_TEST_ID, 20000, 30000, 5000, 3),
    // std::make_tuple(EFM_DETECTION_TEST_ID, 25000, 30000, 5000, 3),
    // std::make_tuple(EFM_DETECTION_TEST_ID, 30000, 30000, 5000, 3),
    // std::make_tuple(EFM_DETECTION_TEST_ID, 35000, 30000, 5000, 3),
    // std::make_tuple(EFM_DETECTION_TEST_ID, 40000, 30000, 5000, 3),
    // std::make_tuple(EFM_DETECTION_TEST_ID, 45000, 30000, 5000, 3),
    // std::make_tuple(EFM_DETECTION_TEST_ID, 50000, 30000, 5000, 3)
  )
);
