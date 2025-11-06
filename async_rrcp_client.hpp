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

#include <atomic>
#include <boost/algorithm/string/predicate.hpp>  // for starts_with
#include <boost/algorithm/string/trim.hpp>  // for trim_left, trim_right
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "rrcp_helper.hpp"

namespace rrcp
{

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

constexpr size_t MAX_LENGTH = 65432;
constexpr auto TIMEOUT_DURATION = 13s;
constexpr auto HEARTBEAT_INTERVAL = 10s;

class async_rrcp_client : public std::enable_shared_from_this< async_rrcp_client >
{
  using message_queue = std::deque< std::string >;
  using signal_string_type = boost::signals2::signal< void(std::string) >;

 public:
  explicit async_rrcp_client(boost::asio::io_context& io_context)
  : io_context_(io_context), socket_(io_context), deadline_(io_context), heartbeat_timer_(io_context)
  {
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
  }

  void start(const tcp::resolver::results_type& endpoints)
  {
    deadline_.expires_after(TIMEOUT_DURATION);
    check_deadline();

    boost::asio::async_connect(socket_, endpoints,
        [self = shared_from_this()](const boost::system::error_code& ec, const tcp::endpoint&) -> void
        {
          if (!ec)
          {
            fmt::print(stderr, "Connected to server.\n");  // TRACE
            self->connected_ = true;
            self->do_read();
            self->send_heartbeat();
          }
          else
          {
            fmt::print(stderr, "Failed to connect: {}\n", ec.message());
          }
        });
  }

  void register_trap_handler(const std::function< void(std::string) >& handler) { trap_handler_.connect(handler); }

  [[nodiscard]] auto connected() const -> bool { return connected_; }

#ifndef USE_OLD_WRITE
  // Asynchronously sends a message and calls handler when the response arrives.
  void async_write_message(
      const std::string& message, std::function< void(const boost::system::error_code&, const std::string&) > handler)
  {
    if (stopped_)
    {
      boost::asio::post(io_context_, [handler]() -> void { handler(boost::asio::error::operation_aborted, {}); });
      return;
    }

    // Generate a unique message ID
    std::string msg_id_str;
    msg_id_ = (msg_id_ + 1) % INVALID_ID;
    auto command = rrcp::create_command_msg(message, msg_id_str, msg_id_);

    auto self = shared_from_this();

    // Post the write to the io_context
    boost::asio::post(io_context_,
        [self, command, msg_id_str, handler = std::move(handler)]() mutable -> void
        {
          bool const write_in_progress = !self->write_msgs_.empty();
          self->write_msgs_.push_back(command);

          // Start the write loop if not already in progress
          if (!write_in_progress)
          {
            self->deadline_.expires_after(TIMEOUT_DURATION);
            self->do_write();
          }

          // Set up a response handler for this specific message ID
          self->pending_responses_[msg_id_str] = handler;
        });
  }

  void stop()
  {
    if (stopped_)
    {
      return;
    }

    stopped_ = true;

    boost::asio::post(io_context_,
        [self = shared_from_this()]() -> void
        {
          boost::system::error_code ec;
          self->socket_.close(ec);
          self->heartbeat_timer_.cancel();
          self->deadline_.cancel();

          // Notify all pending response handlers about the error
          for (auto& [id, handler] : self->pending_responses_)
          {
            handler(boost::asio::error::operation_aborted, {});
          }
          self->pending_responses_.clear();
        });
  }

#else
  // This function write the message into the send msg queue and starts the write actor.
  // It wait for the response message and return this.
  //
  // TODO(CK): we should have two input strings: the MIB name and the command string!
  //
  [[nodiscard]] auto write(const std::string& message) -> std::string
  {
    while (!connected_)
    {
      if (stopped_)
      {
        return {};
      }

      fmt::print(stderr, "Client is not connected yet.\n");  // TRACE
      std::this_thread::sleep_for(TIMEOUT_DURATION);
    }

    std::string msg_id_str;
    msg_id_ = ++msg_id_ % INVALID_ID;
    auto command = rrcp::create_command_msg(message, msg_id_str, msg_id_);

    boost::asio::post(io_context_,
        [this, command]() -> void
        {
          bool const write_in_progress{!write_msgs_.empty()};
          write_msgs_.push_back(command);

          if (!write_in_progress)
          {
            deadline_.expires_after(TIMEOUT_DURATION);
            do_write();
          }
        });

    return read(msg_id_str);
  }

  // This function try to read the response message from the receive msg queue
  auto read(const std::string& msg_id) -> std::string
  {
    std::string response;
    auto count = TIMEOUT_DURATION / 125ms;

    do
    {
      boost::asio::post(io_context_,
          [this, &response]() -> void
          {
            if (!read_msgs_.empty())
            {
              response = read_msgs_.front();
              read_msgs_.pop_front();
            }
          });

      if (!response.empty())
      {
        // helper which returns true if the msg with matching msg_id was found
        if (rrcp::find_response_msg(response, msg_id))
        {
          break;
        }
      }
      std::this_thread::sleep_for(125ms);
    } while (!stopped_ && (--count != 0));
    if (count == 0)
    {
      fmt::print(stderr, "Error: Timeout read!\n");
    }

    return response;
  }

  void stop()
  {
    if (stopped_)
    {
      return;
    }

    boost::asio::post(io_context_,
        [this, self = shared_from_this()]() -> void
        {
          fmt::print(stderr, "Stopped, disconnecting ...\n");
          stopped_ = true;
          connected_ = false;

          boost::system::error_code ec;
          socket_.close(ec);
          heartbeat_timer_.cancel();
          deadline_.cancel();
        });
  }
#endif

 private:
  void do_read()
  {
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(input_buffer_), STOP,
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) -> void
        {
          if (!ec)
          {
            std::string line;
            //========================== RRCP ============================
            if (self->input_buffer_[0] != START)
            {
              fmt::print(stderr, "Warning: Missing START delimiter (found 0x{:02X})\n",
                  static_cast< unsigned char >(self->input_buffer_[0]));
            }
            else
            {
              line = esc2char(self->input_buffer_.substr(1, length - 1));
            }
            self->input_buffer_.erase(0, length);
            //========================== END ============================
            if (line.empty())
            {
              fmt::print(stderr, "Warning: Parsed empty message content - skipping\n");
              self->deadline_.expires_after(HEARTBEAT_INTERVAL + TIMEOUT_DURATION);
              self->do_read();
              return;
            }

            // TODO(CK): maby refactored to helper class?
            //========================== RRCP ============================
            // Process different message types
            if (boost::algorithm::starts_with(line, "d"))  // Trap data message
            {
              // Handle trap data messages
              fmt::print(stderr, "trap data: {}\n", line);  // TRACE
              self->trap_handler_(line);
            }
            else if (!boost::algorithm::starts_with(line, "gPing"))
            {
              // Other responses than Trap and Ping messages
              fmt::print(stderr, "{}\n", line);  // TRACE
              self->handle_response(line);
            }
            // Note: gPing messages are silently ignored (heartbeat responses)
            //========================== END ============================

            self->deadline_.expires_after(HEARTBEAT_INTERVAL + TIMEOUT_DURATION);
            self->do_read();
          }
          else
          {
            fmt::print(stderr, "Error: reading message: {}\n", ec.message());
            self->stop();
          }
        });
  }

  void handle_response(std::string& response)
  {
#ifndef USE_OLD_WRITE
    // Check if this message matches a pending response
    // XXX auto msg_id = rrcp::extract_msg_id(line); // implement helper to extract ID
    // XXX auto it = pending_responses_.find(msg_id);
    // XXX if (it != pending_responses_.end())

    for (auto it = pending_responses_.begin(); it != pending_responses_.end(); ++it)
    {
      if (rrcp::find_response_msg(response, it->first))
      {
        auto handler = std::move(it->second);
        pending_responses_.erase(it);
        // Call handler asynchronously
        boost::asio::post(io_context_, [handler = std::move(handler), response]() -> void { handler({}, response); });
        break;
      }
    }
#else
    read_msgs_.push_back(response);
#endif
  }

  void do_write()
  {
    boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front()),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t /*length*/) -> void
        {
          if (!ec)
          {
            self->write_msgs_.pop_front();
            if (!self->write_msgs_.empty())
            {
              self->do_write();
            }
          }
          else
          {
            // There are no more endpoints to try. Shut down the client.
            fmt::print(stderr, "Error: writing message: {}\n", ec.message());
            self->stop();
          }
        });
  }

  void send_heartbeat()
  {
    if (stopped_)
    {
      return;
    }

    //========================== RRCP ============================
    std::string heartbeat_message{START + char2esc("M:Utility GPing\"async client\"") + STOP};
    //========================== END ============================

    fmt::print(stderr, "Send heartbeat: {}\n", heartbeat_message);  // TRACE
    boost::asio::async_write(socket_, boost::asio::buffer(heartbeat_message),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t) -> void
        {
          if (!ec)
          {
            self->heartbeat_timer_.expires_after(HEARTBEAT_INTERVAL);
            self->heartbeat_timer_.async_wait(
                [self](const boost::system::error_code&) -> void { self->send_heartbeat(); });
          }
          else
          {
            fmt::print(stderr, "Error: sending heartbeat: {}\n", ec.message());
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
      fmt::print(stderr, "No response from server, stopping ...\n");
      stop();
      return;
    }

    deadline_.async_wait(
        [self = shared_from_this()](const boost::system::error_code&) -> void { self->check_deadline(); });
  }

  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  boost::asio::steady_timer deadline_;
  boost::asio::steady_timer heartbeat_timer_;

  // I/O buffers (protected by io_context)
  std::string input_buffer_;
  message_queue write_msgs_;

#ifndef USE_OLD_WRITE
  std::unordered_map< std::string, std::function< void(const boost::system::error_code& ec, std::string) > >
      pending_responses_;
#else
  message_queue read_msgs_;
#endif

  // Thread-safe state
  std::atomic< bool > connected_{false};
  std::atomic< bool > stopped_{false};
  std::atomic< int > msg_id_{10000};

  // Signal handling
  signal_string_type trap_handler_;
};

}  // namespace rrcp
