#pragma once

/***
 * async_rrcp_client.hpp
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * Moderniced from Claus Klein and ChatGPT
 ***/

#include <fmt/format.h>

#include <boost/algorithm/string/predicate.hpp>  // for starts_with
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <thread>

#include "rrcp_helper.hpp"

namespace RRCP
{

using boost::asio::ip::tcp;
using namespace std::chrono_literals;
using message_queue = std::deque< std::string >;

constexpr size_t max_length = 65432;
constexpr auto timeout_duration = 3s;
constexpr auto heartbeat_interval = 10s;

class async_rrcp_client : public std::enable_shared_from_this< async_rrcp_client >
{
 public:
  explicit async_rrcp_client(boost::asio::io_context& io_context)
  : io_context_(io_context), socket_(io_context), deadline_(io_context), heartbeat_timer_(io_context)
  {
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
  }

  void start(const tcp::resolver::results_type& endpoints)
  {
    deadline_.expires_after(timeout_duration);
    check_deadline();

    boost::asio::async_connect(socket_, endpoints,
        [self = shared_from_this()](const boost::system::error_code& ec, const tcp::endpoint&)
        {
          if (!ec)
          {
            fmt::print(stderr, "Connected to server.\n");
            self->connected_ = true;
            self->read();
            self->send_heartbeat();
          }
          else
          {
            fmt::print(stderr, "Failed to connect: {}\n", ec.message());
          }
        });
  }

  auto connected() const -> bool { return connected_; }

  // This function write the message into the msg queue and starts the write actor
  void write(const std::string& message)
  {
    while (!connected_)
    {
      if (stopped_)
      {
        return;
      }
      fmt::print(stderr, "Client is not connected yet.\n");
      std::this_thread::sleep_for(timeout_duration);
    }

    boost::asio::post(io_context_,
        [this, message]()
        {
          bool const write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(message);
          if (!write_in_progress)
          {
            deadline_.expires_after(timeout_duration);
            do_write();
          }
        });
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

            if (!boost::algorithm::starts_with(line, "gPing"))
            {
              fmt::print("{}\n", line);
            }
            self->deadline_.expires_after(heartbeat_interval + timeout_duration);
            self->read();
          }
          else
          {
            fmt::print(stderr, "Error reading message: {}\n", ec.message());
            self->stop();
          }
        });
  }

  void stop()
  {
    fmt::print(stderr, "stop called, disconnecting...\n");
    stopped_ = true;
    connected_ = false;
    boost::system::error_code ec;
    socket_.close(ec);
    heartbeat_timer_.cancel();
    deadline_.cancel();
  }

 private:
  void do_write()
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            fmt::print(stderr, "Message sent.\n");
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            // There are no more endpoints to try. Shut down the client.
            fmt::print(stderr, "Error writing message: {}\n", ec.message());
            stop();
          }
        });
  }

  void send_heartbeat()
  {
    if (stopped_)
    {
      return;
    }

    std::string heartbeat_message{START + char2esc("M:Utility GPing\"ÄÖÜ€0ß\"") + STOP};
    fmt::print(stderr, "Send heartbeat: {}\n", heartbeat_message);
    boost::asio::async_write(socket_, boost::asio::buffer(heartbeat_message),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t)
        {
          if (!ec)
          {
            self->heartbeat_timer_.expires_after(heartbeat_interval);
            self->heartbeat_timer_.async_wait([self](const boost::system::error_code&) { self->send_heartbeat(); });
          }
          else
          {
            fmt::print(stderr, "Error sedning heartbeat: {}\n", ec.message());
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
      fmt::print(stderr, "No response from server, disconnecting...\n");
      stop();
      return;
    }

    deadline_.async_wait([self = shared_from_this()](const boost::system::error_code&) { self->check_deadline(); });
  }

  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  boost::asio::steady_timer deadline_;
  boost::asio::steady_timer heartbeat_timer_;
  std::string input_buffer_;
  message_queue write_msgs_;
  bool connected_{false};
  bool stopped_{false};
};

}  // namespace RRCP
