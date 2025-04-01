#pragma once

#include <string>
#include <string_view>

namespace RRCP
{

constexpr const char START{0x0A};  // \n
constexpr const char STOP{0x0D};  // \r

/**
 * @brief Gets the message between <START>message<STOP> and
 *        replaces the escape sequences for START and STOP
 *
 *
 * @param data: read from socket
 *
 * @return message string like 'M:IBIT SStart'
 */
extern auto esc2char(std::string_view data) -> std::string;

/**
 * @brief Replaces START, STOP with Escape sequence
 *
 * @param data: data to send
 *
 * @return translated data
 */
extern auto char2esc(std::string_view data) -> std::string;

// Here’s a C++17 function that inserts a given string after the first word in an input string,
// where words are separated by WS
inline std::string insertAfterFirstWord(const std::string& input, const std::string& toInsert)
{
  // XXX size_t firstSpace = input.find_first_of(" \t\n\r");  // Find first whitespace
  size_t firstSpace = input.find_first_of(" \t");  // Find first whitespace
  if (firstSpace == std::string::npos)
  {
    return input;  // No spaces found, return original string
  }

  size_t nextNonSpace = input.find_first_not_of(" \t\n", firstSpace);

  // If there's no second word, just append toInsert after the first word
  if (nextNonSpace == std::string::npos)
  {
    return input + " " + toInsert;
  }

  return input.substr(0, firstSpace + 1) + toInsert + " " + input.substr(nextNonSpace);
}

}  // namespace RRCP
