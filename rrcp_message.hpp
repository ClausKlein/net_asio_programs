//
// rrcp_message.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Moderniced from Claus Klein and ChatGPT

#ifndef RRCP_MESSAGE_HPP
#define RRCP_MESSAGE_HPP

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "rrcp_helper.hpp"

using namespace RRCP;

/**
 * @class rrcp_message
 * @brief A message class for handling RRCP protocol messages with a 4-byte header indicating message length.
 */
class rrcp_message
{
 public:
  static constexpr std::size_t header_length = 4;  ///< Fixed length of the message header.
  static constexpr std::size_t max_msg_length = MAX_MU_LENGTH;  ///< Maximum allowed message body length.

  /// Default constructor.
  rrcp_message() = default;

  /**
   * @brief Get a const pointer to the raw data buffer.
   * @return Pointer to the beginning of the message data.
   */
  [[nodiscard]] auto data() const -> const char* { return data(); }

  /**
   * @brief Get a pointer to the raw data buffer.
   * @return Pointer to the beginning of the message data.
   */
  [[nodiscard]] auto data() -> char* { return data_.data(); }

  /**
   * @brief Get the total length of the message (header + body).
   * @return Message length in bytes.
   */
  [[nodiscard]] auto length() const -> std::size_t { return header_length + msg_length_; }

  /**
   * @brief Get a string view of the full message data.
   * @return View of the message data.
   */
  [[nodiscard]] auto get_data() const -> std::string_view { return {data(), length()}; }

  /**
   * @brief Get a const pointer to the message body.
   * @return Pointer to the message body.
   */
  [[nodiscard]] auto body() const -> const char* { return body(); }

  /**
   * @brief Get a pointer to the message body.
   * @return Pointer to the message body.
   */
  [[nodiscard]] auto body() -> char* { return data_.data() + header_length; }

  /**
   * @brief Get the length of the message body.
   * @return Length of the body in bytes.
   */
  [[nodiscard]] auto body_length() const -> std::size_t { return msg_length_; }

  /**
   * @brief Get a string view of the message body.
   * @return View of the message body.
   */
  [[nodiscard]] auto get_body() const -> std::string_view { return {body(), body_length()}; }

  /**
   * @brief Get the decoded message as a string.
   * @return Decoded string from the message body.
   */
  [[nodiscard]] auto get_msg() const -> std::string { return esc2char(std::string(body(), body_length())); }

  /**
   * @brief Set the message body from a string view. Encodes it and sets length.
   * @param msg The message to store.
   * @return True if header was successfully decoded after setting the message.
   */
  [[nodiscard]] auto set_msg(std::string_view msg) -> bool
  {
    std::string data = char2esc(std::string(msg.data(), msg.length()));
    body_length(data.length());
    std::memcpy(body(), data.c_str(), data.length());
    return decode_header();
  }

  /**
   * @brief Set the message body length, constrained by the max message length.
   * @param new_length New length to assign.
   */
  void body_length(std::size_t new_length)
  {
    msg_length_ = new_length;
    msg_length_ = std::min(msg_length_, max_msg_length);
    fmt::print(stderr, "body_length({})\n", msg_length_);
  }

  /**
   * @brief Decode the message body, converting escaped characters.
   * @return True if decoding was successful and not empty.
   */
  auto decode_body() -> bool
  {
    const std::string result = esc2char(std::string(body(), msg_length_));
    if (result.length() != msg_length_)
    {
      fmt::print(stderr, "{}\n", result);

      body_length(result.length());
      std::memcpy(body(), result.c_str(), msg_length_);
    }

    return !result.empty();
  }

  /**
   * @brief Decode the message header to extract the body length.
   * @return True if header is valid, false otherwise.
   */
  auto decode_header() -> bool
  {
    const std::string header(data_.data(), header_length);
    msg_length_ = std::stoul(header, nullptr, 16);

    if (msg_length_ > max_msg_length)
    {
      fmt::print(stderr, "Invalid msg_length!\n");

      msg_length_ = 0;
      return false;
    }

    return true;
  }

  /**
   * @brief Encode the message body by escaping special characters.
   */
  void encode_body()
  {
    std::string msg = char2esc(std::string(body(), msg_length_));
    if (msg.length() != msg_length_)
    {
      body_length(msg.length());
      std::memcpy(body(), msg.c_str(), msg_length_);
    }
  }

  /**
   * @brief Encode the message header with the body length in hexadecimal.
   */
  void encode_header()
  {
    std::string header = fmt::format("{:04x}", static_cast< uint16_t >(msg_length_));
    std::memcpy(data_.data(), header.data(), header_length);
  }

 private:
  std::array< char, header_length + max_msg_length > data_{};  ///< Internal buffer for message data.
  std::size_t msg_length_{0};  ///< Length of the message body.
};

#endif  // RRCP_MESSAGE_HPP
