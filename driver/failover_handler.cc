/**
  @file  failover_handler.c
  @brief Failover functions.
*/

#include "driver.h"
#include "connect.h"
#include <sstream>
#include <regex>

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, TOPOLOGY_SERVICE* topology_service) {
	this->dbc = dbc;
	this->topology_service = topology_service;
	// TODO Change parameters to shared pointers and uncomment the code below
	//this->connection_handler = new FAILOVER_CONNECTION_HANDLER(dbc);
	//this->failover_reader_handler = new FAILOVER_READER_HANDLER(topology_service, connection_handler);
	//this->failover_writer_handler = new FAILOVER_WRITER_HANDLER(topology_service, this->failover_reader_handler);
	this->current_host = nullptr;

	init_cluster_info();
}

void FAILOVER_HANDLER::init_cluster_info() {
	//TODO: implement
}

bool FAILOVER_HANDLER::is_dns_pattern_valid(std::string host) {
	return (host.find("?") != std::string::npos);
}

bool FAILOVER_HANDLER::is_rds_dns(std::string host) {
	static const char* aurora_dns_pattern = "(.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com)";
	return std::regex_match(host, std::regex(aurora_dns_pattern, std::regex_constants::icase));
}

bool FAILOVER_HANDLER::is_rds_proxy_dns(std::string host) {
	static const char* aurora_proxy_dns_pattern = "(.+)\\.(proxy-[a-zA-Z0-9]+\\.[a-zA-Z0-9\\-]+\\.rds\\.amazonaws\\.com)";
	return std::regex_match(host, std::regex(aurora_proxy_dns_pattern, std::regex_constants::icase));
}

bool FAILOVER_HANDLER::is_rds_custom_cluster_dns(std::string host) {
	static const char* aurora_custom_cluster_pattern = "(.+)\\.(cluster-custom-[a-zA-Z0-9]+\\.[a-zA-Z0-9\\-]+\\.rds\\.amazonaws\\.com)";
	return std::regex_match(host, std::regex(aurora_custom_cluster_pattern, std::regex_constants::icase));
}

#ifdef __APPLE__
	#define strcmp_case_insensitive(str1, str2) strcasecmp(str1, str2)
#else
	#define strcmp_case_insensitive(str1, str2) strcmpi(str1, str2)
#endif

std::string FAILOVER_HANDLER::get_rds_cluster_host_url(std::string host) {
	static const char* aurora_dns_pattern = "(.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com)";
	std::smatch m;
	if (std::regex_search(host, m, std::regex(aurora_dns_pattern, std::regex_constants::icase)) && m.size() > 1) {
		std::string gr1 = m.size() > 1 ? m.str(1) : std::string("");
		std::string gr2 = m.size() > 2 ? m.str(2) : std::string("");
		std::string gr3 = m.size() > 3 ? m.str(3) : std::string("");
		if (!gr1.empty() && !gr3.empty() && 
			(strcmp_case_insensitive(gr2.c_str(), "cluster-") == 0 || strcmp_case_insensitive(gr2.c_str(), "cluster-ro-") == 0)) {
			std::string result;
			result.assign(gr1);
			result.append(".cluster-");
			result.append(gr3);

			return result;
		}
	}
	return "";
}

std::string FAILOVER_HANDLER::get_rds_instance_host_pattern(std::string host) {
	static const char* aurora_dns_pattern = "(.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com)";
	std::smatch m;
	if (std::regex_search(host, m, std::regex(aurora_dns_pattern, std::regex_constants::icase)) && m.size() > 3) {
		if (!m.str(3).empty()) {
			std::string result("?.");
			result.append(m.str(3));

			return result;
		}
	}
	return "";
}

bool FAILOVER_HANDLER::is_failover_enabled() {
	return (dbc != nullptr && dbc->ds != nullptr &&
		!dbc->ds->disable_cluster_failover &&
		is_cluster_topology_available &&
		!is_rds_proxy &&
		!is_multi_writer_cluster);
}

void FAILOVER_HANDLER::create_connection_and_initialize_topology() {
	//TODO: implement
}

bool FAILOVER_HANDLER::is_ipv4(std::string host) {
	static const char* ipv4_pattern = "^(([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){1}(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){2}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$";
	return std::regex_match(host, std::regex(ipv4_pattern));
}

bool FAILOVER_HANDLER::is_ipv6(std::string host) {
	static const char* ipv6_pattern = "^[0-9a-fA-F]{1,4}(:[0-9a-fA-F]{1,4}){7}$";
	static const char* ipv6_compressed_pattern = "^(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)::(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)$";
	return std::regex_match(host, std::regex(ipv6_pattern)) || std::regex_match(host, std::regex(ipv6_compressed_pattern));
}

bool FAILOVER_HANDLER::trigger_failover_if_needed(const char* error_code, const char*& new_error_code) {
	// TODO: implement
	return false;
}

bool FAILOVER_HANDLER::failover_to_reader(const char*& new_error_code) {
	// TODO: implement
	return false;
}

bool FAILOVER_HANDLER::failover_to_writer(const char*& new_error_code) {
	// TODO: implement
	return false;
}

void FAILOVER_HANDLER::refresh_topology() {
	//TODO: implement
}

FAILOVER_HANDLER::~FAILOVER_HANDLER() {
	if (connection_handler != NULL) {
		delete connection_handler;
	}
}
