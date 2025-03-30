//
// rrcp_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Moderniced from Claus Klein and ChatGPT

#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cassert>
#include <chrono>  // NOLINT(misc-include-cleaner)
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "rrcp_helper.hpp"
#include "rrcp_message.hpp"

using boost::asio::ip::tcp;

using rrcp_message_queue = std::deque< rrcp_message >;

class rrcp_client
{
 public:
  rrcp_client(boost::asio::io_context& io_context, const tcp::resolver::results_type& endpoints)
  : io_context_(io_context), socket_(io_context)
  {
    do_connect(endpoints);
  }

  void write(const rrcp_message& msg)
  {
    boost::asio::post(io_context_,
        [this, msg]()
        {
          bool const write_in_progress{!write_msgs_.empty()};
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

  void close()
  {
    boost::asio::post(io_context_, [this]() { write_msgs_.clear(); });
    boost::asio::post(io_context_, [this]() { socket_.close(); });
  }

 private:
  void do_connect(const tcp::resolver::results_type& endpoints)
  {
    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, const tcp::endpoint&)
        {
          if (!ec)
          {
            do_read_header();
          }
        });
  }

  void do_read_header()
  {
    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), rrcp_message::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_read_body()
  {
    boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_body())
          {
            // NOLINTNEXTLINE(bugprone-narrowing-conversions)
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << "\n";
            do_read_header();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_write()
  {
    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            socket_.close();
          }
        });
  }

  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  rrcp_message read_msg_;
  rrcp_message_queue write_msgs_;
};

auto main(int argc, char* argv[]) -> int
{
  using namespace std::chrono_literals;
  using namespace std::string_literals;

  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: rrcp_client <host> <port>\n";
      return EXIT_FAILURE;
    }

    boost::asio::io_context io_context;

    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);
    rrcp_client c(io_context, endpoints);

    std::thread t([&io_context]() { io_context.run(); });

    //================================================================
    std::string binary{"\nAB_(\0\001\002\003\004\005\006\a\b\n\r\t\v\x1b\20\'\"\?)-CD\r"s};
    std::cerr << binary.length() << ' ' << std::quoted(binary) << '\n';
    auto quoted = char2esc(binary);
    std::cerr << quoted.length() << ' ' << std::quoted(quoted) << '\n';

    assert(binary == esc2char(quoted));
    assert(binary.length() < quoted.length());
    assert(binary.length() == 28);
    assert(quoted.length() == 33);
    //================================================================

    int i{};
    std::this_thread::sleep_for(100ms);  // NOLINT(misc-include-cleaner)
    for (std::string line; std::getline(std::cin, line);)
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

      std::cerr << ++i << '\t' << line << '\n';
      rrcp_message msg;
      msg.body_length(line.length());
      std::memcpy(msg.body(), line.c_str(), msg.body_length());
      msg.encode_body();
      msg.encode_header();
      c.write(msg);
    }
    std::this_thread::sleep_for(100ms);  // NOLINT(misc-include-cleaner)

    c.close();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
