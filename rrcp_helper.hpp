#pragma once

#include <string>

/**
 * @brief Gets the message between <LF>message<CR> and
 *        replaces the escape sequences for LF and CR
 *
 *
 * @param data: read from socket
 *
 * @return message string like 'M:IBIT SStart'
 */
extern std::string esc2char(const std::string& data);

/**
 * @brief Replaces LF, CR with Escape sequence
 *
 * @param data:  data to send
 *
 * @return translated data
 */
extern std::string char2esc(const std::string& data);
