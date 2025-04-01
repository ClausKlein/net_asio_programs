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
// XXX #include <string_view>

#include "rrcp_helper.hpp"

using namespace RRCP;

class rrcp_message
{
 public:
  static constexpr std::size_t header_length = 4;
  static constexpr std::size_t max_msg_length = MAX_MU_LENGTH;

  rrcp_message() = default;

  // TODO(CK): or better const &std::string_view?
  [[nodiscard]] auto data() const -> const char* { return data_.data(); }

  auto data() -> char* { return data_.data(); }

  [[nodiscard]] auto length() const -> std::size_t { return header_length + msg_length_; }

  // TODO(CK): or better const &std::string_view?
  [[nodiscard]] auto body() const -> const char* { return data_.data() + header_length; }

  auto body() -> char* { return data_.data() + header_length; }

  [[nodiscard]] auto body_length() const -> std::size_t { return msg_length_; }

  void body_length(std::size_t new_length)
  {
    msg_length_ = new_length;
    msg_length_ = std::min(msg_length_, max_msg_length);
    fmt::print(stderr, "body_length({})\n", msg_length_);
  }

  auto decode_body() -> bool
  {
    // TODO(CK): or better const &std::string_view?
    auto result = esc2char(std::string(body(), msg_length_));
    if (result.length() != msg_length_)
    {
      fmt::print(stderr, "{}\n", result);

      body_length(result.length());
      std::memcpy(body(), result.c_str(), msg_length_);
    }

    return !result.empty();
  }

  auto decode_header() -> bool
  {
    const std::string header(data_.data(), header_length);
    msg_length_ = std::stoul(header, nullptr, 16);

    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    if (msg_length_ > max_msg_length)
    {
      fmt::print(stderr, "Invalid msg_length!\n");

      msg_length_ = 0;
      return false;
    }

    return true;
  }

  void encode_body()
  {
    // TODO(CK): or better const &std::string_view?
    auto msg = char2esc(std::string(body(), msg_length_));
    if (msg.length() != msg_length_)
    {
      body_length(msg.length());
      std::memcpy(body(), msg.c_str(), msg_length_);
    }
  }

  void encode_header()
  {
    std::string header = fmt::format("{:04x}", static_cast< uint16_t >(msg_length_));
    std::memcpy(data_.data(), header.data(), header_length);
  }

 private:
  std::array< char, header_length + max_msg_length > data_{};
  std::size_t msg_length_{0};
};

#endif  // RRCP_MESSAGE_HPP
