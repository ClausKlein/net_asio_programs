//
// blocking_tcp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Moderniced from Claus Klein and ChatGPT

#include <boost/algorithm/string/trim.hpp>  // for trim_right
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "rrcp_helper.hpp"

using boost::asio::ip::tcp;

static constexpr int max_length{1024};

auto main(int argc, char* argv[]) -> int
{
  try
  {
    using namespace rrcp;

    if (argc != 3)
    {
      std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
      return EXIT_FAILURE;
    }

    boost::asio::io_context io_context;

    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(s, resolver.resolve(argv[1], argv[2]));

    for (std::string line; std::getline(std::cin, line); std::cerr << "Enter command: ")
    {
      const std::string::size_type sz = line.find("//");
      if ((sz != std::string::npos))
      {
        line.resize(sz);
      }

      boost::trim_right(line);
      if (line.empty())
      {
        continue;
      }

      // TODO(CK): check boost::system::error_code ec;
      std::string command = char2esc(line);
      command.insert(0, 1, START);
      command += STOP;
      boost::asio::write(s, boost::asio::buffer(command.c_str(), command.length()));

      // TODO(CK): wait for endchar with timeout!
      std::string data;
      boost::asio::dynamic_string_buffer< char, std::string::traits_type, std::string::allocator_type > const sb2 =
          boost::asio::dynamic_buffer(data, max_length);

      do
      {
        size_t const reply_length = boost::asio::read_until(s, sb2, STOP);  // NOLINT(clang-analyzer-deadcode.DeadStores)
        std::string const response = esc2char(data);
        if (response.empty())
        {
          break;
        }

        std::cerr << "Response is: ";
        std::cout << response << "\n";
      } while (false);
    };
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
