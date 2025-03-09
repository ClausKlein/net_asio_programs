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

#include <cstdio>
#include <cstdlib>
#include <cstring>

class rrcp_message
{
 public:
  static constexpr std::size_t header_length = 4;
  static constexpr std::size_t max_msg_length = 65432;

  rrcp_message() : msg_length_(0) {}

  const char* data() const { return data_; }

  char* data() { return data_; }

  std::size_t length() const { return header_length + msg_length_; }

  const char* body() const { return data_ + header_length; }

  char* body() { return data_ + header_length; }

  std::size_t body_length() const { return msg_length_; }

  void body_length(std::size_t new_length)
  {
    msg_length_ = new_length;
    if (msg_length_ > max_msg_length) msg_length_ = max_msg_length;
  }

  bool decode_header()
  {
    char header[header_length + 1] = "";
    std::strncat(header, data_, header_length);
    msg_length_ = std::atoi(header);
    if (msg_length_ > max_msg_length)
    {
      msg_length_ = 0;
      return false;
    }
    return true;
  }

  void encode_header()
  {
    char header[header_length + 1] = "";
    std::snprintf(
        header, header_length + 1, "%4d", static_cast< int >(msg_length_));
    std::memcpy(data_, header, header_length);
  }

 private:
  char data_[header_length + max_msg_length];
  std::size_t msg_length_;
};

#endif  // RRCP_MESSAGE_HPP
