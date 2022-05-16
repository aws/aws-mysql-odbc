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

#ifndef __MYLOG_H__
#define __MYLOG_H__

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

#define MYLOG_STMT_TRACE(A, B)                                              \
  {                                                                         \
    if ((A)->dbc->ds->save_queries)                                         \
      trace_print((A)->dbc->log_file.get(), (A)->dbc->id, (const char *)B); \
  }

#define MYLOG_DBC_TRACE(A, ...) \
  { trace_print((A)->log_file.get(), (A)->id, __VA_ARGS__); }

#define MYLOG_TRACE(A, B, ...)                            \
  {                                                       \
    if ((A) != nullptr) trace_print((A), B, __VA_ARGS__); \
  }

// stateless functor object for deleting FILE handle
struct FILEDeleter {
  void operator()(FILE *file) {
    if (file) {
      fclose(file);
      file = nullptr;
    }
  }
};

static std::shared_ptr<FILE> log_file;
static std::mutex log_file_mutex;

/* Functions used when debugging */
std::shared_ptr<FILE> init_log_file();
void end_log_file();
void trace_print(FILE *file, unsigned long dbc_id, const char *fmt, ...);

#endif /* __MYLOG_H__ */
