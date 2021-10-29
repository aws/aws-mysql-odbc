/**
  @file connect.h
  @brief Definitions needed for connect
*/

#ifndef __CONNECT_H__
#define __CONNECT_H__

#include <stddef.h>
#include <vector>

struct Srv_host_detail
{
	std::string name;
	unsigned int port = MYSQL_PORT;
};

std::vector<Srv_host_detail> parse_host_list(const char* hosts_str,
	unsigned int default_port);

#endif /* __CONNECT_H__ */
