//
// blocking_tcp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "rrcp_helper.hpp"

#include <boost/asio.hpp>
#include <boost/asio/read_until.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

using boost::asio::ip::tcp;

enum
{
  max_length = 1024
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(s, resolver.resolve(argv[1], argv[2]));

    do
    {
      std::cout << "Enter message: ";
      char request[max_length];
      std::cin.getline(request, max_length);
      size_t request_length = std::strlen(request);
      if (request_length == 0) break;

      std::string data = char2esc(std::string(request, request_length));

      boost::asio::write(s, boost::asio::buffer(data.c_str(), data.length()));

      // TODO: wait for endchar with timeout!
      std::string reply;
      boost::asio::dynamic_string_buffer< char, std::string::traits_type,
          std::string::allocator_type >
          sb2 = boost::asio::dynamic_buffer(reply, max_length);
      boost::system::error_code ec;

      size_t reply_length{};
      do
      {
        reply_length = boost::asio::read_until(s, sb2, ';');
        std::cout << "Reply is: ";
        std::cout.write(reply.c_str(), reply_length);
        std::cout << "\n";
      } while (reply_length > 0);
    } while (true);
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
