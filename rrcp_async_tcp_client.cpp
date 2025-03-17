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

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "rrcp_helper.hpp"

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

class rrcp_client : public std::enable_shared_from_this< rrcp_client >
{
 public:
  explicit rrcp_client(boost::asio::io_context& io_context)
  : socket_(io_context), deadline_(io_context), heartbeat_timer_(io_context)
  {
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
  }

  void start(const tcp::resolver::results_type& endpoints)
  {
    deadline_.expires_after(3s);
    check_deadline();

    boost::asio::async_connect(socket_, endpoints,
        [self = shared_from_this()](const boost::system::error_code& ec, const tcp::endpoint&)
        {
          if (!ec)
          {
            std::cout << "Connected to server.\n";
            self->read();
            self->send_heartbeat();
          }
          else
          {
            // FIXME: this aborts! CK
            throw std::runtime_error("Failed to connect: " + ec.message());
          }
        });
  }

  void write()
  {
    std::string message;
    while (std::getline(std::cin, message))
    {
      if (stopped_)
      {
        return;
      }

      message = START + char2esc(message) + STOP;
      boost::asio::async_write(socket_, boost::asio::buffer(message),
          [self = shared_from_this()](const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              std::cerr << "Error sending message: " << ec.message() << '\n';
              self->stop();
            }
          });

      deadline_.expires_after(3s);
    }
  }

  void read()
  {
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(input_buffer_), STOP,
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t length)
        {
          if (!ec)
          {
            std::string const line = esc2char(self->input_buffer_.substr(1, length - 1));  // w/o START, STOP
            self->input_buffer_.erase(0, length);

            if (!line.starts_with("gPing"))
            {
              std::cout << line << '\n';
            }
            self->read();
            self->deadline_.expires_after(13s);
          }
          else
          {
            std::cerr << "Error reading message: " << ec.message() << '\n';
            self->stop();
          }
        });
  }

  void stop()
  {
    std::cerr << "stop called, disconnecting...\n";
    stopped_ = true;
    boost::system::error_code ec;
    socket_.close(ec);
    heartbeat_timer_.cancel();
    deadline_.cancel();
  }

 private:
  void send_heartbeat()
  {
    if (stopped_)
    {
      return;
    }

    std::string heartbeat_message{START + char2esc("M:Utility GPing") + STOP};
    std::cerr << "Send heartbeat: " << heartbeat_message << '\n';
    boost::asio::async_write(socket_, boost::asio::buffer(heartbeat_message),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t)
        {
          if (!ec)
          {
            self->heartbeat_timer_.expires_after(10s);
            self->heartbeat_timer_.async_wait([self](const boost::system::error_code&) { self->send_heartbeat(); });
          }
          else
          {
            std::cerr << "Error sending heartbeat: " << ec.message() << '\n';
            self->stop();
          }
        });
  }

  void check_deadline()
  {
    if (stopped_)
    {
      return;
    }

    if (deadline_.expiry() <= boost::asio::steady_timer::clock_type::now())
    {
      std::cerr << "No response from server, disconnecting...\n";
      stop();
      return;
    }

    deadline_.async_wait([self = shared_from_this()](const boost::system::error_code&) { self->check_deadline(); });
  }

  tcp::socket socket_;
  boost::asio::steady_timer deadline_, heartbeat_timer_;
  std::string input_buffer_;
  bool stopped_{false};
};

auto main(int argc, char* argv[]) -> int
{
  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
    return EXIT_FAILURE;
  }

  try
  {
    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);

    auto c = std::make_shared< rrcp_client >(io_context);
    c->start(resolver.resolve(argv[1], argv[2]));

    std::thread io_thread([&io_context]() { io_context.run(); });

    std::this_thread::sleep_for(3s);  // NOLINT(misc-include-cleaner)
    c->write();

    c->stop();
    io_thread.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
