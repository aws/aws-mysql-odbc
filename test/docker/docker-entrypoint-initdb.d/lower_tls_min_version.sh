# Driver test my_basics:t_tls_opts needs a system configured to accept at least TLSv1.0
echo "-- Lowering Minimum TLS Protocol supported, for ODBC driver tests"
sed 's/TLSv1.2/TLSv1.0/g' /etc/ssl/openssl.cnf > /tmp/openssl.cnf
echo "-- New OpenSSL config now in /tmp/openssl.cnf"

