//
// rrcp_message.hpp
// ~~~~~~~~~~~~~~~~
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

class rrcp_message
{
 public:
  static constexpr std::size_t header_length = 4;
  static constexpr std::size_t max_msg_length = 65432;

  rrcp_message() {}

  [[nodiscard]] const char* data() const { return data_.data(); }

  char* data() { return data_.data(); }

  [[nodiscard]] std::size_t length() const
  {
    return header_length + msg_length_;
  }

  [[nodiscard]] const char* body() const
  {
    return data_.data() + header_length;
  }

  char* body() { return data_.data() + header_length; }

  [[nodiscard]] std::size_t body_length() const { return msg_length_; }

  void body_length(std::size_t new_length)
  {
    msg_length_ = new_length;
    msg_length_ = std::min(msg_length_, max_msg_length);
  }

  bool decode_header()
  {
    std::string header(data_.data(), header_length);
    msg_length_ = std::stoi(header);
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
    std::snprintf(header.data(), header_length + 1, "%4d",
        static_cast< int >(msg_length_));
    std::memcpy(data_.data(), header.data(), header_length);
  }

 private:
  std::array< char, header_length + max_msg_length > data_;
  std::size_t msg_length_{0};
};

#endif  // RRCP_MESSAGE_HPP
