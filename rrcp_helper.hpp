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

}  // namespace RRCP
