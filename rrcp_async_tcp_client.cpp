/***
 * rrcp_async_tcp_client.cpp
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * Moderniced from Claus Klein and ChatGPT
 ***/

#include <fmt/format.h>

#include <boost/algorithm/string/trim.hpp>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

// optional #define USE_SIMPLE_RRCP_CLIENT
#ifdef USE_SIMPLE_RRCP_CLIENT
#include "async_rrcp_client.hpp"
#else
#include "async_rrcp_client_threadsafe.hpp"
#endif

namespace
{

void print(std::string msg) { fmt::print("{}\n", msg); }

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char* argv[]) -> int
{
  if (argc < 3)
  {
    fmt::print(stderr, "Usage: {} <host> <port> [input_file]\n", argv[0]);  // NOLINT
    return EXIT_FAILURE;
  }

  std::ifstream file;  // persistent file object (if used)
  std::istream* input_str = &std::cin;  // pointer to chosen input stream

  if (argc == 4)
  {
    file.open(argv[3]);  // NOLINT
    if (!file)
    {
      fmt::print(stderr, "cannot open input file: {}\n", argv[3]);  // NOLINT
      return 2;
    }
    input_str = &file;
  }

  try
  {
    using namespace rrcp;

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);

    auto client = std::make_shared< async_rrcp_client >(io_context);
    client->register_trap_handler(&print);
    client->start(resolver.resolve(argv[1], argv[2]));

    std::thread io_thread([&io_context]() -> void { io_context.run(); });

    std::this_thread::sleep_for(TIMEOUT_DURATION);  // NOTE: only for gcov results! CK
    for (std::string line; client->connected() && std::getline(*input_str, line); fmt::print(stderr, "Enter command: "))
    {
      const std::string::size_type sz = line.find("//");
      if ((sz != std::string::npos))
      {
        line.resize(sz);  // NOTE: w/o c++ comments
      }

      boost::trim(line);
      if (line.empty())
      {
        continue;
      }

      if (boost::algorithm::starts_with(line, "E:"))
      {
        continue;
      }

#if defined(USE_SIMPLE_RRCP_CLIENT) && !defined(USE_OLD_WRITE)
      client->async_write_message(line,
          [](const boost::system::error_code& ec, const std::string& response)
          {
            if (!ec)
            {
              fmt::print("Received response: {}\n", response);
            }
            else
            {
              fmt::print("Error: {}\n", ec.message());
            }
          });
#else
      const auto response = client->write(line);
      fmt::print("{}\n", response);
#endif
    }
    std::this_thread::sleep_for(HEARTBEAT_INTERVAL);  // NOTE: only for gcov results! CK

    client->stop();
    io_thread.join();
  }
  catch (const std::exception& e)
  {
    fmt::print(stderr, "Exception: {}\n", e.what());  // NOLINT
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
