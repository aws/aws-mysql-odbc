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

#include "driver/query_parsing.h"

// Helper function to advance the index of query_str to after the closing quotation.
// The passed in index is assumed to be the position of the opening quotation mark.
// Any escaped quotation marks will be ignored.
void scan_quotation(const std::string& query_str, size_t& index, char quotation_mark)
{
    bool escaped = false;
    while (++index < query_str.size())
    {
        if (query_str[index] == '\\')
        {
            escaped = !escaped;
        }
        else
        {
            // The quotation mark must not be escaped for it to be a valid
            // closing quotation mark.
            if (!escaped && query_str[index] == quotation_mark)
            {
                index++;
                return;
            }

            escaped = false;
        }
    }
}

// Helper function to determine if the character at position index in query_str
// (which is assumed to be ' ', '/r', '/n', or '/t' and outside of any quotations)
// can be safely removed from the statement without changing the meaning of the statement.
bool is_character_unnecessary(const std::string& query_str, const size_t& index)
{
    // The character is unnecessary if it's the first or last character.
    if (index == 0 || index == query_str.size() - 1)
    {
        return true;
    }

    // The character is unnecessary if the next character is also similar 
    // to space or is a statement separator (ie. ;).
    switch (query_str[index + 1])
    {
    case ' ':
    case '\r':
    case '\n':
    case '\t':
    case ';':
        return true;
    default:
        return false;
    }
}

/**
  Splits up user provided query text into a vector of individual
  statements (separated by ; in the original query).
  Each statement is cleaned of any unnecessary characters.

  @param[in] original_query    The query being processed
  */
std::vector<std::string> parse_query_into_statements(const char* original_query)
{
    std::vector<std::string> statements;
    std::string query_str(original_query);

    size_t index = 0;
    while (index < query_str.size())
    {
        switch (query_str[index])
        {
        // If we find a quotation then advance the index
        // to after the closing quotation.
        case '\"':
        case '\'':
            scan_quotation(query_str, index, query_str[index]);
            break;

        // If we find a space-like character then either remove it
        // or replace it with a space.
        case ' ':
        case '\r':
        case '\n':
        case '\t':
            if (is_character_unnecessary(query_str, index))
            {
                query_str.erase(index, 1);
            }
            else
            {
                if (query_str[index] != ' ')
                {
                    query_str.replace(index, 1, " ");
                }
                index++;
            }
            break;

        // If we find the statement separator ; then we are ready to 
        // build our statement string.
        case ';':
            if (index > 0)
            {
                std::string statement = query_str.substr(0, index);
                statements.push_back(statement);
            }

            query_str = query_str.substr(index + 1, query_str.size() - 1);
            index = 0;
            break;

        default:
            index++;
        }
    }

    // If we still have any query_str left then it must be 
    // the final statement with no separator.
    if (index > 0 && index == query_str.size())
    {
        statements.push_back(query_str);
    }

    return statements;
}
