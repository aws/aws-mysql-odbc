echo "-- Lowering Minimum TLS Protocol supported, for ODBC driver tests"
sed 's/TLSv1.2/TLSv1.0/g' /etc/ssl/openssl.cnf > /tmp/openssl.cnf
echo "-- New OpenSSL config now in /tmp/openssl.cnf"
# mv /tmp/openssl.cnf /etc/ssl/openssl.cnf
