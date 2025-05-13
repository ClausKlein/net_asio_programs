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
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "async_rrcp_client.hpp"

namespace
{

void print(std::string msg) { fmt::print("{}\n", msg); }

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char* argv[]) -> int
{
  if (argc != 3)
  {
    fmt::print(stderr, "Usage: {} <host> <port>\n", argv[0]);  // NOLINT
    return EXIT_FAILURE;
  }

  try
  {
    using namespace rrcp;

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);

    auto client = std::make_shared< async_rrcp_client >(io_context);
    client->register_trap_handler(&print);
    client->start(resolver.resolve(argv[1], argv[2]));

    std::thread io_thread([&io_context]() { io_context.run(); });

    std::this_thread::sleep_for(TIMEOUT_DURATION);  // NOTE: only for gcov results! CK
    for (std::string line; client->connected() && std::getline(std::cin, line); fmt::print(stderr, "Enter command: "))
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

      const auto response = client->write(line);
      fmt::print("{}\n", response);
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
