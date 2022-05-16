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

#include "mylog.h"

#include <cstdarg>
#include <ctime>
#include <vector>

#include "driver.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

void trace_print(FILE *file, unsigned long dbc_id, const char *fmt, ...) {
  if (file && fmt) {
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

#ifdef _WIN32
    int pid;
    pid = _getpid();
#else
    pid_t pid;
    pid = getpid();
#endif

    fprintf(file, "%s - Process ID %ld -  DBC ID %lu - %s\n", time_buf, pid,
            dbc_id, buf.data());
    fflush(file);
  }
}

std::shared_ptr<FILE> init_log_file() {
  if (log_file) 
    return log_file;

  std::lock_guard<std::mutex> guard(log_file_mutex);
  if (log_file) 
    return log_file;

  FILE *file;
#ifdef _WIN32
  char filename[MAX_PATH];
  size_t buffsize;

  getenv_s(&buffsize, filename, sizeof(filename), "TEMP");

  if (buffsize) {
    sprintf(filename + buffsize - 1, "\\%s", DRIVER_LOG_FILE);
  } else {
    sprintf(filename, "c:\\%s", DRIVER_LOG_FILE);
  }

  if (file = fopen(filename, "a+"))
#else
  if (file = fopen(DRIVER_LOG_FILE, "a+"))
#endif
  {
    fprintf(file, "-- Driver logging\n");
    fprintf(file, "--\n");
    fprintf(file, "--  Driver name: %s  Version: %s\n", DRIVER_NAME,
            DRIVER_VERSION);
#ifdef HAVE_LOCALTIME_R
    {
      time_t now = time(nullptr);
      struct tm start;
      localtime_r(&now, &start);

      fprintf(file, "-- Timestamp: %02d%02d%02d %2d:%02d:%02d\n",
              start.tm_year % 100, start.tm_mon + 1, start.tm_mday,
              start.tm_hour, start.tm_min, start.tm_sec);
    }
#endif /* HAVE_LOCALTIME_R */
    fprintf(file, "\n");
    log_file = std::shared_ptr<FILE>(file, FILEDeleter());
  }
  return log_file;
}

void end_log_file() {
  std::lock_guard<std::mutex> guard(log_file_mutex);
  if (log_file && log_file.use_count() == 1) { // static var
    log_file.reset();
  }
}
