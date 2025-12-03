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
#include <cxxopts.hpp>
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

// a little helper to return all args
struct options
{
  uint16_t port{8000};
  uint32_t timeout{3};
  bool verbose{true};
  std::string server{"localhost"};
  std::string filename{"-"};
};

options getcxxopts(int argc, char** argv)
{
  options opt;
  try
  {
    cxxopts::Options options(*argv, "rrcp");

    // clang-format off
        options.add_options()
                ("h,help", "this help")
                ("t,timeout", "Message timeout", cxxopts::value<uint32_t>()->default_value("3"))
                ("v,verbose", "trace notification handling", cxxopts::value<bool>()->default_value("true"))
                ("p,port", "rrcpPort, default 8000", cxxopts::value<uint16_t>()->default_value(std::to_string(opt.port)))
                ("s,server", "Server name to connect", cxxopts::value<std::string>()->default_value(opt.server))
                ("f,file", "File name to read from", cxxopts::value<std::string>()->default_value(opt.filename));
    // clang-format on

    auto result = options.parse(argc, argv);
    if (result.contains("help"))
    {
      std::cout << options.help() << '\n';
      exit(EXIT_SUCCESS);  // NOLINT(concurrency-mt-unsafe)
    }
    if (result.contains("timeout"))
    {
      opt.timeout = result["timeout"].as< uint32_t >();
    }
    if (result.contains("verbose"))
    {
      opt.verbose = result["verbose"].as< bool >();
    }
    if (result.contains("port"))
    {
      opt.port = result["port"].as< uint16_t >();
    }
    if (result.contains("server"))
    {
      opt.server = result["server"].as< std::string >();
    }
    if (result.contains("file"))
    {
      opt.filename = result["file"].as< std::string >();
    }
  }
  catch (const cxxopts::exceptions::exception& e)
  {
    fmt::print(stderr, "cxxopt parse exception: {}", e.what());
    exit(EXIT_FAILURE);  // NOLINT(concurrency-mt-unsafe)
  }
  return opt;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char* argv[]) -> int
{
  auto opts = getcxxopts(argc, argv);

  std::ifstream file;  // persistent file object (if used)
  std::istream* input_str = &std::cin;  // pointer to chosen input stream

  if (opts.filename != "-")
  {
    file.open(opts.filename);
    if (!file)
    {
      fmt::print(stderr, "cannot open input file: {}\n", opts.filename);  // NOLINT
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
    client->start(resolver.resolve(opts.server, std::to_string(opts.port)));

    std::thread io_thread([&io_context]() -> void { io_context.run(); });

    std::this_thread::sleep_for(opts.timeout * 500ms);  // NOTE: only for gcov results! CK
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
          [](const boost::system::error_code& ec, const std::string& response) -> void
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
    std::this_thread::sleep_for(opts.timeout * 500ms);  // NOTE: only for gcov results! CK

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
