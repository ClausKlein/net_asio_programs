//
// rrcp_message_buffer.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef RRCP_MESSAGE_HPP
#define RRCP_MESSAGE_HPP

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

class rrcp_message
{
 public:
  static constexpr std::size_t header_length = 4;
  static constexpr std::size_t max_msg_length = 65432;

  rrcp_message() {}

  // TODO: or better const &std::string_view?
  [[nodiscard]] const std::string_view data() const { return data_; }

  std::string data() { return data_; }

  [[nodiscard]] std::size_t length() const
  {
    return header_length + msg_length_;
  }

  // TODO: or better const &std::string_view?
  [[nodiscard]] const std::string body() const
  {
    return data_.substr(header_length);
  }

  std::string body() { return data_.substr(header_length); }

  [[nodiscard]] std::size_t body_length() const { return msg_length_; }

  void body_length(std::size_t new_length)
  {
    msg_length_ = new_length;
    msg_length_ = std::min(msg_length_, max_msg_length);
  }

  bool decode_header()
  {
    std::string header(data_, header_length);
    msg_length_ = std::stoul(header, nullptr, 16);
    if (msg_length_ > max_msg_length)
    {
      msg_length_ = 0;
      return false;
    }
    return true;
  }

  void encode_header()
  {
    std::array< char, header_length + 1 > header;
    std::snprintf(header.data(), header_length + 1, "%4x",
        static_cast< uint16_t >(msg_length_));
    data_.substr(0, header_length) = std::string(header.data(), header_length);
  }

 private:
  std::array< char, header_length + max_msg_length > data_;
  // TODO: used std::string data_;
  std::size_t msg_length_{0};
};

#endif  // RRCP_MESSAGE_HPP
