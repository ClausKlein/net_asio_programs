#pragma once

/***
 * async_rrcp_client_threadsafe.hpp
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * Thread-safe implementation with preserved interface
 ***/

#include <fmt/format.h>

#include <atomic>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "rrcp_helper.hpp"

namespace rrcp
{

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

constexpr size_t MAX_LENGTH = 65432;
constexpr auto TIMEOUT_DURATION = 3s;
constexpr auto HEARTBEAT_INTERVAL = 10s;

class async_rrcp_client : public std::enable_shared_from_this< async_rrcp_client >
{
  using message_queue = std::deque< std::string >;
  using signal_string_type = boost::signals2::signal< void(std::string) >;
  using response_promise_type = std::promise< std::string >;

 public:
  explicit async_rrcp_client(boost::asio::io_context& io_context)
  : io_context_(io_context),
    strand_(boost::asio::make_strand(io_context)),
    socket_(strand_),
    deadline_(strand_),
    heartbeat_timer_(strand_)
  {
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
  }

  void start(const tcp::resolver::results_type& endpoints)
  {
    boost::asio::post(strand_,
        [this, endpoints, self = shared_from_this()]() -> void
        {
          deadline_.expires_after(TIMEOUT_DURATION);
          check_deadline();

          boost::asio::async_connect(socket_, endpoints,
              [self](const boost::system::error_code& ec, const tcp::endpoint&) -> void
              {
                if (!ec)
                {
                  fmt::print(stderr, "Connected to server.\n");
                  self->connected_.store(true);
                  self->notify_connection_waiters();
                  self->do_read();
                  self->send_heartbeat();
                }
                else
                {
                  fmt::print(stderr, "Failed to connect: {}\n", ec.message());
                  self->notify_connection_waiters();
                }
              });
        });
  }

  void register_trap_handler(const std::function< void(std::string) >& handler)
  {
    boost::asio::post(strand_, [this, handler, self = shared_from_this()]() -> void { trap_handler_.connect(handler); });
  }

  [[nodiscard]] auto connected() const -> bool { return connected_.load(); }

  // Thread-safe synchronous write with preserved interface
  [[nodiscard]] auto write(const std::string& message) -> std::string
  {
    auto promise = std::make_shared< response_promise_type >();
    auto future = promise->get_future();

    boost::asio::post(strand_,
        [this, message, promise, self = shared_from_this()]() -> void
        {
          if (!connected_.load())
          {
            // Queue request until connected
            pending_writes_.emplace_back(message, promise);
            return;
          }

          execute_write_request(message, promise);
        });

    // Wait for response with timeout
    if (future.wait_for(TIMEOUT_DURATION) == std::future_status::timeout)
    {
      return {};  // Timeout - return empty string
    }

    try
    {
      const auto response = future.get();
      fmt::print(stderr, "Returning {}\n", response);
      return response;
    }
    catch (const std::exception&)
    {
      return {};  // Error - return empty string
    }
  }

  void stop()
  {
    boost::asio::post(strand_,
        [this, self = shared_from_this()]() -> void
        {
          fmt::print(stderr, "Stopped, disconnecting ...\n");
          stopped_.store(true);
          connected_.store(false);

          boost::system::error_code ec;
          socket_.close(ec);
          heartbeat_timer_.cancel();
          deadline_.cancel();

          // Clear pending operations and notify waiters
          clear_pending_operations();
          notify_connection_waiters();
        });
  }

 private:
  // Helper method to safely parse RRCP message with bounds checking
  static auto parse_rrcp_message(const std::string& buffer, std::size_t length, std::string& parsed_line) -> bool
  {
    // Minimum RRCP message: START + at least 1 char + STOP = 3 bytes
    if (length < 3)
    {
      fmt::print(stderr, "Warning: Message too short (length={}) - expected minimum 3 bytes\n", length);
      return false;
    }

    // Validate buffer size
    if (buffer.size() < length)
    {
      fmt::print(stderr, "Error: Buffer size ({}) smaller than expected length ({})\n", buffer.size(), length);
      return false;
    }

    // Check for START delimiter at beginning
    if (buffer[0] != START)
    {
      fmt::print(stderr, "Warning: Missing START delimiter (found 0x{:02X})\n", static_cast< unsigned char >(buffer[0]));
      return false;
    }

    // Check for STOP delimiter at expected position
    if (buffer[length - 1] != STOP)
    {
      fmt::print(stderr, "Warning: Missing STOP delimiter at position {} (found 0x{:02X})\n", length - 1,
          static_cast< unsigned char >(buffer[length - 1]));
      return false;
    }

    // Extract message content (without START and STOP)
    if (length >= 3)
    {
      parsed_line = esc2char(buffer.substr(1, length - 2));
      return true;
    }

    return false;
  }

  void execute_write_request(const std::string& message, std::shared_ptr< response_promise_type > promise)
  {
    std::string msg_id_str;
    int current_id = next_message_id_.fetch_add(1);
    auto command = rrcp::create_command_msg(message, msg_id_str, current_id);

    // Store promise for response correlation
    pending_responses_[msg_id_str] = std::move(promise);

    bool const write_in_progress{!write_msgs_.empty()};
    write_msgs_.push_back(command);

    if (!write_in_progress)
    {
      deadline_.expires_after(TIMEOUT_DURATION);
      do_write();
    }
  }

  void notify_connection_waiters()
  {
    // Process pending writes that were waiting for connection
    for (auto& [message, promise] : pending_writes_)
    {
      if (connected_.load())
      {
        execute_write_request(message, promise);
      }
      else
      {
        // Connection failed - fulfill promise with empty response
        try
        {
          promise->set_value("");
        }
        // NOLINTNEXTLINE(bugprone-empty-catch)
        catch (const std::exception&)
        {
          // Promise already fulfilled - ignore
        }
      }
    }
    pending_writes_.clear();
  }

  void clear_pending_operations()
  {
    // Fulfill all pending promises with empty responses
    for (auto& [msg_id, promise] : pending_responses_)
    {
      try
      {
        promise->set_value("");
      }
      // NOLINTNEXTLINE(bugprone-empty-catch)
      catch (const std::exception&)
      {
        // Promise already fulfilled - ignore
      }
    }
    pending_responses_.clear();

    for (auto& [message, promise] : pending_writes_)
    {
      try
      {
        promise->set_value("");
      }
      // NOLINTNEXTLINE(bugprone-empty-catch)
      catch (const std::exception&)
      {
        // Promise already fulfilled - ignore
      }
    }
    pending_writes_.clear();

    write_msgs_.clear();
  }

  void do_read()
  {
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(input_buffer_), STOP,
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) -> void
        {
          if (!ec)
          {
            std::string parsed_line;

            // Use safe parsing helper with comprehensive bounds checking
            if (!rrcp::async_rrcp_client::parse_rrcp_message(self->input_buffer_, length, parsed_line))
            {
              // Parsing failed - message was malformed, skip it
              self->input_buffer_.erase(0, length);
              self->deadline_.expires_after(HEARTBEAT_INTERVAL + TIMEOUT_DURATION);
              self->do_read();
              return;
            }

            // Successfully parsed, remove processed data from buffer
            self->input_buffer_.erase(0, length);

            // Validate parsed content is not empty
            if (parsed_line.empty())
            {
              fmt::print(stderr, "Warning: Parsed empty message content - skipping\n");
              self->deadline_.expires_after(HEARTBEAT_INTERVAL + TIMEOUT_DURATION);
              self->do_read();
              return;
            }

            // Process different message types
            if (boost::algorithm::starts_with(parsed_line, "d"))
            {
              // Handle trap data messages
              fmt::print(stderr, "trap data: {}\n", parsed_line);
              self->trap_handler_(parsed_line);
            }
            else if (!boost::algorithm::starts_with(parsed_line, "gPing"))
            {
              // Handle response messages (but ignore heartbeat responses)
              fmt::print(stderr, "{}\n", parsed_line);
              self->handle_response(parsed_line);
            }
            // Note: gPing messages are silently ignored (heartbeat responses)

            self->deadline_.expires_after(HEARTBEAT_INTERVAL + TIMEOUT_DURATION);
            self->do_read();
          }
          else
          {
            fmt::print(stderr, "Error reading message: {}\n", ec.message());
            self->stop();
          }
        });
  }

  void handle_response(const std::string& response)
  {
    // Find matching pending response
    for (auto it = pending_responses_.begin(); it != pending_responses_.end(); ++it)
    {
      std::string clean_response = response;
      if (rrcp::find_response_msg(clean_response, it->first))
      {
        auto promise = it->second;
        pending_responses_.erase(it);

        // Fulfill promise with response
        try
        {
          promise->set_value(clean_response);
        }
        // NOLINTNEXTLINE(bugprone-empty-catch)
        catch (const std::exception&)
        {
          // Promise already fulfilled - ignore
        }
        return;
      }
    }
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
            fmt::print(stderr, "Error writing message: {}\n", ec.message());
            self->stop();
          }
        });
  }

  void send_heartbeat()
  {
    if (stopped_.load())
    {
      return;
    }

    std::string heartbeat_message{START + char2esc("M:Utility GPing\"async client\"") + STOP};

    fmt::print(stderr, "Send heartbeat: {}\n", heartbeat_message);
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
            fmt::print(stderr, "Error sending heartbeat: {}\n", ec.message());
            self->stop();
          }
        });
  }

  void check_deadline()
  {
    if (stopped_.load())
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

  // Core networking components
  boost::asio::io_context& io_context_;
  boost::asio::strand< boost::asio::io_context::executor_type > strand_;
  tcp::socket socket_;
  boost::asio::steady_timer deadline_;
  boost::asio::steady_timer heartbeat_timer_;

  // I/O buffers (protected by strand)
  std::string input_buffer_;
  message_queue write_msgs_;

  // Thread-safe state
  std::atomic< bool > connected_{false};
  std::atomic< bool > stopped_{false};
  std::atomic< int > next_message_id_{10000};

  // Response correlation system (protected by strand)
  std::unordered_map< std::string, std::shared_ptr< response_promise_type > > pending_responses_;
  std::vector< std::pair< std::string, std::shared_ptr< response_promise_type > > > pending_writes_;

  // Signal handling
  signal_string_type trap_handler_;
};

}  // namespace rrcp
