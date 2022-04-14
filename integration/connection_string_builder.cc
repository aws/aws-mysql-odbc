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

#include <stdexcept>
#include <string>
#include <memory>

class ConnectionStringBuilder;

class ConnectionString {
  public:
    // friend class so the builder can access ConnectionString private attributes
    friend class ConnectionStringBuilder;

    ConnectionString() : m_dsn(""), m_server(""), m_port(-1),
                         m_uid(""), m_pwd(""), m_db(""), m_log_query(true), m_allow_reader_connections(false),
                         m_multi_statements(false), m_failover_t(-1), m_connect_timeout(-1), m_network_timeout(-1),
                         m_host_pattern(""),
                         
                         is_set_uid(false), is_set_pwd(false), is_set_db(false), is_set_log_query(false),
                         is_set_allow_reader_connections(false), is_set_multi_statements(false), is_set_failover_t(false),
                         is_set_connect_timeout(false), is_set_network_timeout(false), is_set_host_pattern(false) {};

    std::string get_connection_string() const {
      char conn_in[4096] = "\0";
      int length = 0;
      length += sprintf(conn_in, "DSN=%s;SERVER=%s;PORT=%d;", m_dsn.c_str(), m_server.c_str(), m_port);

      if (is_set_uid) {
        length += sprintf(conn_in + length, "UID=%s;", m_uid.c_str()); 
      }
      if (is_set_pwd) {
        length += sprintf(conn_in + length, "PWD=%s;", m_pwd.c_str()); 
      }
      if (is_set_db) {
        length += sprintf(conn_in + length, "DATABASE=%s;", m_db.c_str()); 
      }
      if (is_set_log_query) {
        length += sprintf(conn_in + length, "LOG_QUERY=%d;", m_log_query ? 1 : 0); 
      }
      if (is_set_allow_reader_connections) {
        length += sprintf(conn_in + length, "ALLOW_READER_CONNECTIONS=%d;", m_allow_reader_connections ? 1 : 0); 
      }
      if (is_set_multi_statements) {
        length += sprintf(conn_in + length, "MULTI_STATEMENTS=%d;", m_multi_statements ? 1 : 0); 
      }
      if (is_set_failover_t) {
        length += sprintf(conn_in + length, "FAILOVER_T=%d;", m_failover_t); 
      }
      if (is_set_connect_timeout) {
        length += sprintf(conn_in + length, "CONNECT_TIMEOUT=%d;", m_connect_timeout); 
      }
      if (is_set_network_timeout) {
        length += sprintf(conn_in + length, "NETWORK_TIMEOUT=%d;", m_network_timeout); 
      }
      if (is_set_host_pattern) {
        length += sprintf(conn_in + length, "HOST_PATTERN=%s;", m_host_pattern.c_str()); 
      }
      sprintf(conn_in + length, "\0");

      std::string connection_string(conn_in);
      return connection_string;
    }
    
  private:
    // Required fields
    std::string m_dsn, m_server;
    int m_port;

    // Optional fields
    std::string m_uid, m_pwd, m_db;
    bool m_log_query, m_allow_reader_connections, m_multi_statements;
    int m_failover_t, m_connect_timeout, m_network_timeout;
    std::string m_host_pattern;

    bool is_set_uid, is_set_pwd, is_set_db;
    bool is_set_log_query, is_set_allow_reader_connections, is_set_multi_statements;
    bool is_set_failover_t, is_set_connect_timeout, is_set_network_timeout;
    bool is_set_host_pattern;

    void set_dsn(const std::string& dsn) {
      m_dsn = dsn;
    }

    void set_server(const std::string& server) {
      m_server = server;
    }

    void set_port(const int& port) {
      m_port = port;
    }

    void set_uid(const std::string& uid) {
      m_uid = uid;
      is_set_uid = true;
    }

    void set_pwd(const std::string& pwd) {
      m_pwd = pwd;
      is_set_pwd = true;
    }

    void set_db(const std::string& db) {
      m_db = db;
      is_set_db = true;
    }

    void set_log_query(const bool& log_query) {
      m_log_query = log_query;
      is_set_log_query = true;
    }

    void set_allow_reader_connections(const bool& allow_reader_connections) {
      m_allow_reader_connections = allow_reader_connections;
      is_set_allow_reader_connections = true;
    }

    void set_multi_statements(const bool& multi_statements) {
      m_multi_statements = multi_statements;
      is_set_multi_statements = true;
    }

    void set_failover_t(const int& failover_t) {
      m_failover_t = failover_t;
      is_set_failover_t = true;
    }

    void set_connect_timeout(const int& connect_timeout) {
      m_connect_timeout = connect_timeout;
      is_set_connect_timeout = true;
    }

    void set_network_timeout(const int& network_timeout) {
      m_network_timeout = network_timeout;
      is_set_network_timeout = true;
    }

    void set_host_pattern(const std::string& host_pattern) {
      m_host_pattern = host_pattern;
      is_set_host_pattern = true;
    }
};

class ConnectionStringBuilder {
  public:
    ConnectionStringBuilder() {
      connection_string.reset(new ConnectionString());
    }
    
    ConnectionStringBuilder& withDSN(const std::string& dsn) {
      connection_string->set_dsn(dsn);
      return *this;
    }

    ConnectionStringBuilder& withServer(const std::string& server) {
      connection_string->set_server(server);
      return *this;
    }

    ConnectionStringBuilder& withPort(const int& port) {
      connection_string->set_port(port);
      return *this;
    }
    
    ConnectionStringBuilder& withUID(const std::string& uid) {
      connection_string->set_uid(uid);
      return *this;
    }

    ConnectionStringBuilder& withPWD(const std::string& pwd) {
      connection_string->set_pwd(pwd);
      return *this;
    }

    ConnectionStringBuilder& withDatabase(const std::string& db) {
      connection_string->set_db(db);
      return *this;
    }

    ConnectionStringBuilder& withLogQuery(const bool& log_query) {
      connection_string->set_log_query(log_query);
      return *this;
    }

    ConnectionStringBuilder& withAllowReaderConnections(const bool& allow_reader_connections) {
      connection_string->set_allow_reader_connections(allow_reader_connections);
      return *this;
    }

    ConnectionStringBuilder& withMultiStatements(const bool& multi_statements) {
      connection_string->set_multi_statements(multi_statements);
      return *this;
    }

    ConnectionStringBuilder& withFailoverT(const int& failover_t) {
      connection_string->set_failover_t(failover_t);
      return *this;
    }

    ConnectionStringBuilder& withConnectTimeout(const int& connect_timeout) {
      connection_string->set_connect_timeout(connect_timeout);
      return *this;
    }

    ConnectionStringBuilder& withNetworkTimeout(const int& network_timeout) {
      connection_string->set_network_timeout(network_timeout);
      return *this;
    }

    ConnectionStringBuilder& withHostPattern(const std::string& host_pattern) {
      connection_string->set_host_pattern(host_pattern);
      return *this;
    }

    std::string build() const {
      if (connection_string->m_dsn.empty()) {
        throw std::runtime_error("DSN is a required field in a connection string.");
      }
      if (connection_string->m_server.empty()) {
        throw std::runtime_error("Server is a required field in a connection string.");
      }
      if (connection_string->m_port < 1) {
        throw std::runtime_error("Port is a required field in a connection string.");
      }
      return connection_string->get_connection_string();
    }
    
  private:
    std::unique_ptr<ConnectionString> connection_string;
};
