/**
  @file query_parsing.h
  @brief Methods for parsing user queries
*/

#ifndef __QUERY_PARSING_H__
#define __QUERY_PARSING_H__

#include <string>
#include <vector>

std::vector<std::string> parse_query_into_statements(const char* original_query);

#endif /* __QUERY_PARSING_H__ */
