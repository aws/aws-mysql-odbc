/*
 * Copyright (c) 2008, 2023, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0, as
 * published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an
 * additional permission to link the program and your derivative works
 * with the separately licensed software that they have included with
 * MySQL.
 *
 * Without limiting anything contained in the foregoing, this file,
 * which is part of MySQL Connector/C++, is also subject to the
 * Universal FOSS Exception, version 1.0, a copy of which can be found at
 * http://oss.oracle.com/licenses/universal-foss-exception.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "telemetry.h"
#include <VersionInfo.h>
#include "driver.h"

#include <iostream>
#include <vector>


namespace telemetry
{
  Span_ptr mk_span(
    std::string name, trace::SpanContext *link = nullptr
  )
  {
    auto tracer = trace::Provider::GetTracerProvider()->GetTracer(
      "MySQL Connector/ODBC " MYODBC_STRDRIVERTYPE, MYODBC_CONN_ATTR_VER
    );
  
    trace::StartSpanOptions opts;
    opts.kind = trace::SpanKind::kClient;

    auto span
    = link ? tracer->StartSpan(name, {}, {{*link, {}}},  opts)
           : tracer->StartSpan(name, opts);

    span->SetAttribute("db.system", "mysql");
    return span;
  }

  Span_ptr mk_span(std::string name, trace::SpanContext link)
  {
    return mk_span(name, &link);
  }


  Span_ptr 
  Telemetry_base<DBC>::mk_span(DBC *conn)
  {
    return telemetry::mk_span("connection");
  }

  template<>
  bool 
  Telemetry_base<STMT>::disabled(STMT *stmt) const
  {
    return stmt->conn_telemetry().disabled(stmt->dbc);
  }


  /*
    Creating statement span: we link it to the connection span and we also
    set "traceparent" attribute unless user already set it.
  */
 
  template<>
  Span_ptr 
  Telemetry_base<STMT>::mk_span(STMT *stmt)
  {
    auto span = telemetry::mk_span("SQL statement",
      stmt->conn_telemetry().span->GetContext()
    );

    if (!stmt->query_attr_exists("traceparent"))
    {
      char buf[trace::TraceId::kSize * 2];
      auto ctx = span->GetContext();

      ctx.trace_id().ToLowerBase16(buf);
      std::string trace_id{buf, sizeof(buf)};

      ctx.span_id().ToLowerBase16({buf, trace::SpanId::kSize * 2});
      std::string span_id{buf, trace::SpanId::kSize * 2};

      stmt->add_query_attr(
        "traceparent", "00-" + trace_id + "-" + span_id + "-00"
      );
    }

    return span;
  }

}
