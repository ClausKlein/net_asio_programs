#pragma once

#include <string>
#include <string_view>

namespace RRCP
{

constexpr const char START{0x0A};  // \n
constexpr const char STOP{0x0D};  // \r
constexpr const int INVALID_ID{16777216};  // valid range is 0 to 2**24 - 1
constexpr const size_t MAX_MU_LENGTH{65432};

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

// A C++17 function that inserts a given string after the first word in an input string,
// where words are separated by WS
extern auto insertAfterFirstWord(const std::string& input, const std::string& toInsert) -> std::string;

// helper which returns true if the msg with matching msg_id was found
extern auto find_response_msg(std::string& response, const std::string& msg_id) -> bool;

// helper which returns the command msg with next valid msg_id inserted if needed
extern auto create_command_msg(const std::string& message, std::string& msg_id_str, int& msg_id) -> std::string;

}  // namespace RRCP
