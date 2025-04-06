//
// rrcp_message.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff
// Moderniced from Claus Klein and ChatGPT
//

#ifndef RRCP_MESSAGE_HPP
#define RRCP_MESSAGE_HPP

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "rrcp_helper.hpp"

using namespace RRCP;

/**
 * @class rrcp_message
 * @brief Encapsulates an RRCP message with methods for encoding, decoding, and accessing message content.
 *
 * Each message consists of a 4-byte header (hex-encoded length) and an escaped string payload.
 */
class rrcp_message
{
 public:
  /// Number of bytes used for the fixed-size header.
  static constexpr std::size_t header_length = 4;

  /// Maximum message body length in bytes.
  static constexpr std::size_t max_msg_length = MAX_MU_LENGTH;

  /**
   * @brief Default constructor.
   */
  rrcp_message() = default;

  /**
   * @brief Construct a message from a string view.
   * @param msg The message content.
   *
   * The message is encoded and the header is generated. Validity is tracked.
   */
  explicit rrcp_message(std::string_view msg)
  {
    valid_ = set_msg(msg);
    if (!valid_)
    {
      clear();
    }
  }

  /**
   * @brief Checks if the message is in a valid state.
   * @return True if valid, false otherwise.
   */
  [[nodiscard]] auto is_valid() const -> bool { return valid_; }

  /**
   * @brief Returns a const pointer to the start of the raw message buffer.
   * @return Pointer to buffer (includes header and body).
   */
  [[nodiscard]] auto data() const -> const char* { return data_.data(); }

  /**
   * @brief Returns a mutable pointer to the start of the raw message buffer.
   * @return Pointer to buffer (includes header and body).
   */
  [[nodiscard]] auto data() -> char* { return data_.data(); }

  /**
   * @brief Returns the total length of the message (header + body).
   * @return Total length in bytes.
   */
  [[nodiscard]] auto length() const -> std::size_t { return header_length + msg_length_; }

  /**
   * @brief Returns a view over the full message buffer.
   * @return Message as a std::string_view.
   */
  [[nodiscard]] auto get_data() const -> std::string_view { return {data(), length()}; }

  /**
   * @brief Returns a const pointer to the message body.
   * @return Pointer to the body (excludes header).
   */
  [[nodiscard]] auto body() const -> const char* { return data_.data() + header_length; }

  /**
   * @brief Returns a mutable pointer to the message body.
   * @return Pointer to the body (excludes header).
   */
  [[nodiscard]] auto body() -> char* { return data_.data() + header_length; }

  /**
   * @brief Returns the length of the message body in bytes.
   * @return Length of the body.
   */
  [[nodiscard]] auto body_length() const -> std::size_t { return msg_length_; }

  /**
   * @brief Returns a view of the message body.
   * @return Message body as a std::string_view.
   */
  [[nodiscard]] auto get_body() const -> std::string_view { return {body(), body_length()}; }

  /**
   * @brief Returns the decoded message string (after escaping is removed).
   * @return Decoded message.
   */
  [[nodiscard]] auto get_msg() const -> std::string { return esc2char(std::string(body(), body_length())); }

  /**
   * @brief Sets the message body using the input string, escaping it as needed.
   * @param msg The input message.
   * @return True if header encoding is successful, false otherwise.
   */
  [[nodiscard]] auto set_msg(std::string_view msg) -> bool
  {
    std::string data = char2esc(std::string{msg});
    body_length(data.length());

    if (msg_length_ > max_msg_length)
    {
#ifdef DEBUG
      fmt::print(stderr, "{}: {} to long!\n", __func__, msg_length_);
#endif
      clear();
      return false;
    }

    std::memcpy(body(), data.c_str(), msg_length_);
    encode_header();
    return valid_;
  }

  /**
   * @brief Sets the body length, clamping it to the maximum allowed length.
   * @param new_length Desired length of body.
   */
  void body_length(std::size_t new_length)
  {
    msg_length_ = std::min(new_length, max_msg_length);
#ifdef DEBUG
    fmt::print(stderr, "body_length({})\n", msg_length_);
#endif
  }

  /**
   * @brief Decodes the escaped content in the body.
   * @return True if decoding succeeds, false if result is empty.
   */
  auto decode_body() -> bool
  {
    // TODO(CK): only if (msg_length != 0 && not yet done)!
    const std::string result = esc2char(std::string(body(), msg_length_));
    if (result.length() != msg_length_)
    {
#ifdef DEBUG
      fmt::print(stderr, "{}: {}\n", __func__, result);
#endif
      body_length(result.length());
      std::memcpy(body(), result.c_str(), msg_length_);
    }

    return !result.empty();
  }

  /**
   * @brief Parses the 4-byte header to determine body length.
   * @return True if the header is valid, false if the length is out of bounds.
   */
  auto decode_header() -> bool
  {
    // TODO(CK): only if msg_length != 0
    const std::string header(data_.data(), header_length);
    msg_length_ = std::stoul(header, nullptr, 16);

    if (msg_length_ > max_msg_length)
    {
#ifdef DEBUG
      fmt::print(stderr, "{}: Invalid msg_length {}!\n", __func__, header);
#endif
      msg_length_ = 0;
      return false;
    }

    return true;
  }

  /**
   * @brief Encodes the body to escaped format and adjusts body length.
   */
  void encode_body()
  {
    // TODO(CK): only if (msg_length != 0 && not yet done)!
    std::string msg = char2esc(std::string(body(), msg_length_));
    if (msg.length() != msg_length_)
    {
      body_length(msg.length());
      std::memcpy(body(), msg.c_str(), msg_length_);
    }
  }

  /**
   * @brief Encodes the message header from the current body length.
   */
  void encode_header()
  {
    // TODO(CK): only if msg_length != 0
    std::string header = fmt::format("{:04x}", static_cast< uint16_t >(msg_length_));
    std::memcpy(data_.data(), header.data(), header_length);
    valid_ = true;
  }

  /**
   * @brief Clears the message buffer and resets internal state.
   */
  void clear()
  {
    msg_length_ = 0;
    valid_ = false;
    data_.fill('\0');
  }

 private:
  std::array< char, header_length + max_msg_length > data_{};  ///< Internal buffer for message (header + body).
  std::size_t msg_length_{0};  ///< Length of message body.
  bool valid_{false};  ///< Flag indicating whether the message is valid.
};

#endif  // RRCP_MESSAGE_HPP
