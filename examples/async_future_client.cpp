/***
 * async_future_client.cpp
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * Modernized from Claus Klein and ChatGPT
 ***/

#include <fmt/format.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <format>
#include <future>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

/**
 * @brief Protocol Requirements and Design Overview
 *
 * 1. Each request message starts with ASCII message number (zero-padded to 5 digits).
 * 2. Messages prepend a 4-byte ASCII length header.
 * 3. Responses follow the same format and must match the request msg_num.
 * 4. The client stores pending requests in a map: std::map<std::string, std::promise<std::string>> pending_;
 * 5. The receiver reads the length header, reads the exact message body, extracts msg_num,
 *    and fulfills the corresponding promise in the map.
 */
using namespace std::chrono_literals;
using boost::asio::ip::tcp;

/**
 * @brief Structure representing a response from the server
 */
struct response
{
  std::string msg_num_; /**< ASCII message number from server */
  std::vector< uint8_t > data_; /**< Payload bytes */
};

/**
 * @brief Asynchronous TCP client supporting future-based requests
 *
 * This client:
 * - Sends requests asynchronously.
 * - Reads responses and fulfills promises associated with the message number.
 * - Supports multiple concurrent outstanding requests.
 */
class async_future_client : public std::enable_shared_from_this< async_future_client >
{
 public:
  /**
   * @brief Construct a new async_future_client
   * @param ctx Reference to an existing Boost.Asio io_context
   * @param host Hostname or IP address of the server
   * @param port TCP port of the server
   */
  async_future_client(boost::asio::io_context& ctx, const std::string& host, const std::string& port)
  : io_context_(ctx), resolver_(ctx), socket_(ctx)
  {
    resolver_.async_resolve(host, port,
        [this](boost::system::error_code ec, const tcp::resolver::results_type& results) -> void
        {
          if (!ec)
          {
            boost::asio::async_connect(socket_, results,
                [this](boost::system::error_code ec, const tcp::endpoint&) -> void
                {
                  if (!ec)
                  {
                    fmt::print(stderr, "Connected to server.\n");
                    connected_ = true;
                    do_read_length();
                  }
                  else
                  {
                    // No more endpoints to try; shut down client
                    stop();
                  }
                });
          }
        });
  }

  /**
   * @brief Send a request payload asynchronously
   *
   * The function returns a std::future<response> which will be set once
   * the corresponding response arrives from the server.
   *
   * @param payload Vector of bytes to send
   * @return std::future<response> Future to get the response
   */
  auto send_request(const std::vector< uint8_t >& payload) -> std::future< response >
  {
#ifdef HAS_ATOMIC_THREAD_FENCE
    uint16_t num = msg_counter_.fetch_add(1, std::memory_order_relaxed) % std::numeric_limits< uint16_t >::max();
#else
    uint16_t num{};
    {
      std::scoped_lock const lock(map_mutex_);
      num = msg_counter_ + 1;
      num = num % std::numeric_limits< uint16_t >::max();
      msg_counter_ = num;
    }
#endif

    std::string msg_num = fmt::format("{:05}", num);  // 5-digit ASCII message number

    // Build message: [msg_num + payload]
    std::vector< uint8_t > body;
    body.insert(body.end(), msg_num.begin(), msg_num.end());
    body.insert(body.end(), payload.begin(), payload.end());

    // Prepend 4-byte ASCII length header
    std::string len_str = fmt::format("{:04}", body.size());
    std::vector< uint8_t > full_msg;
    full_msg.insert(full_msg.end(), len_str.begin(), len_str.end());
    full_msg.insert(full_msg.end(), body.begin(), body.end());

    // Prepare promise and future
    std::promise< response > prom;
    auto fut = prom.get_future();
    {
      std::scoped_lock const lock(map_mutex_);
      pending_.emplace(msg_num, std::move(prom));
    }

    size_t counter{4};
    while (!connected_)
    {
      if (stopped_)
      {
        return fut;
      }
      fmt::print(stderr, "Client is not connected yet.\n");
      if (--counter == 0U)
      {
        return fut;
      }
      std::this_thread::sleep_for(125ms * counter);
    }

    fmt::print("Send msg ({})\n", std::string(full_msg.begin(), full_msg.end()));

    boost::asio::async_write(socket_, boost::asio::buffer(full_msg),
        [this, msg_num](std::error_code ec, std::size_t /*n*/) -> void
        {
          if (ec)
          {
            fmt::print(stderr, "Write Error: send_request({}) {}\n", msg_num, ec.message());

            std::scoped_lock const lock(map_mutex_);
            auto it = pending_.find(msg_num);
            if (it != pending_.end())
            {
              it->second.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
              pending_.erase(it);
            }
            else
            {
              fmt::print(stderr, "Error: num not found at send_request({}) !\n", msg_num);
            }
          }
        });

    return fut;
  }

  /**
   * @brief Stop the client and close the socket
   *
   * This will cancel all ongoing operations and prevent new requests.
   */
  void stop()
  {
    if (stopped_)
    {
      return;
    }

    boost::asio::post(io_context_,
        [this]() -> void
        {
          stopped_ = true;
          connected_ = false;
          boost::system::error_code ignored_error;
          socket_.close(ignored_error);
        });
  }

  /**
   * @brief Example test function that repeatedly sends a payload
   *
   * Demonstrates how to use send_request() and wait for the future.
   */
  void test()
  {
    std::vector< uint8_t > const payload{'H', 'e', 'l', 'l', 'o', ' ', 'T', 'e', 's', 't'};
    do
    {
      auto fut = send_request(payload);
      if (fut.wait_for(1s) == std::future_status::ready)
      {
        auto resp = fut.get();
        fmt::print("Received response ({}): {}\n", resp.msg_num_, std::string(resp.data_.begin(), resp.data_.end()));
      }
      else
      {
        fmt::print(stderr, "Timeout waiting for response\n");
        break;
      }
    } while (true);
  }

 private:
  /**
   * @brief Start reading the 4-byte length header
   */
  void do_read_length()
  {
    boost::asio::async_read(socket_, boost::asio::buffer(len_buf_),
        [this](std::error_code ec, std::size_t /*n*/) -> void
        {
          if (!ec)
          {
            std::string const len_str(len_buf_.begin(), len_buf_.end());
            std::size_t const msg_len = std::stoul(len_str, nullptr, 10);
            do_read_body(msg_len);
          }
          else
          {
            fmt::print(stderr, "Read length Error: do_read_length() {}\n", ec.message());
            // XXX std::runtime_error(ec.message());
            // There are no more endpoints to try. Silently shut down the client.
            stop();
          }
        });
  }

  /**
   * @brief Read the message body after reading the length header
   * @param msg_len Length of the message body
   */
  void do_read_body(std::size_t msg_len)
  {
    fmt::print(stderr, "do_read_body({})\n", msg_len);

    body_buf_.resize(msg_len);
    boost::asio::async_read(socket_, boost::asio::buffer(body_buf_),
        [this, msg_len](std::error_code ec, std::size_t /*n*/) -> void
        {
          if (!ec && body_buf_.size() >= 5)
          {
            std::string const msg_num(body_buf_.begin(), body_buf_.begin() + 5);
            std::vector< uint8_t > payload(body_buf_.begin() + 5, body_buf_.end());
            handle_response(msg_num, std::move(payload));
            do_read_length();
          }
          else if (ec)
          {
            fmt::print(stderr, "Read body Error: do_read_body({}) {}\n", msg_len, ec.message());
            std::runtime_error(ec.message());
          }
        });
  }

  /**
   * @brief Fulfill the promise associated with the received message number
   * @param msg_num Message number as ASCII string
   * @param data Payload data received
   */
  void handle_response(const std::string& msg_num, std::vector< uint8_t > data)
  {
    fmt::print(stderr, "do_read_body({}) {}\n", msg_num,
        std::string_view(reinterpret_cast< const char* >(data.data()), data.size()));

    std::scoped_lock const lock(map_mutex_);
    auto it = pending_.find(msg_num);
    if (it != pending_.end())
    {
      it->second.set_value(response{msg_num, std::move(data)});
      pending_.erase(it);
    }
    else
    {
      fmt::print(stderr, "Error: num not found at do_read_body({}) !\n", msg_num);
    }
  }

  boost::asio::io_context& io_context_; /**< Reference to Boost.Asio io_context */
  tcp::resolver resolver_; /**< Resolver for hostname lookup */
  tcp::socket socket_; /**< TCP socket */
  std::array< char, 4 > len_buf_{}; /**< Buffer for 4-byte length header */
  std::vector< uint8_t > body_buf_; /**< Buffer for message body */

#ifdef HAS_ATOMIC_THREAD_FENCE
  std::atomic< uint16_t > msg_counter_{0}; /**< Message counter (atomic) */
#else
  uint16_t msg_counter_{0}; /**< Message counter (protected by mutex) */
#endif

  std::atomic< bool > connected_{false}; /**< Flag: connection established */
  std::atomic< bool > stopped_{false}; /**< Flag: client stopped */

  std::mutex map_mutex_; /**< Protects access to pending_ map */
  std::map< std::string, std::promise< response > > pending_; /**< Map of pending requests */
};

/**
 * @brief Example
 */
auto main(int argc, char* argv[]) -> int
{
  if (argc != 3)
  {
    fmt::print("Usage: {} <host> <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  std::string const host = argv[1];
  std::string const port = argv[2];

  try
  {
    boost::asio::io_context ctx;
    auto client = std::make_shared< async_future_client >(ctx, host, port);

    std::thread io_thread([&ctx] -> void { ctx.run(); });

    std::this_thread::sleep_for(1s);

    std::thread test1(&async_future_client::test, client);
    // TODO(CK) std::thread test2(&async_future_client::test, client);

    test1.join();
    // TODO(CK) test2.join();

    ctx.stop();
    io_thread.join();
  }
  catch (const std::exception& ex)
  {
    fmt::print(stderr, "Exception: {}\n", ex.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
