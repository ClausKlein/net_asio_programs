/***
 * async_future_client.cpp
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * Moderniced from Claus Klein and ChatGPT
 ***/

#include <atomic>
#include <boost/asio.hpp>
#include <cstdlib>
#include <format>
#include <future>
#include <limits>
#include <map>
#include <mutex>
#include <print>
#include <string>
#include <thread>
#include <vector>

/***
======================================================================
# Protocol Requirements

1. Each request message starts with ASCII message number, e.g. "00042"
   (zero-padded to 5 digits or similar - we'll define this).
2. Before the message body (including that message number),
   we send a 4-byte ASCII length header, e.g. "0123"means 123 bytes follow.
3. When a response is received, it has the same structure:
        [4 ASCII bytes: length][ASCII msg_num][payload...]
4. The response msg_num must match the request's.

#  Revised Design Overview

* The client:
    * Builds the ASCII length header + ASCII message number.
    * Sends the whole message.
    * Each request number is stored in a map:
        std::map<std::string, std::promise<std::string>> pending_;
* The receiver:
    * First reads the 4-byte ASCII length header.
    * Then reads exactly that many bytes (the message body).
    * Extracts the ASCII message number (first N chars of the body).
    * Matches it in the map and fulfills the promise.
======================================================================
***/

using namespace std::chrono_literals;
using boost::asio::ip::tcp;

struct Response
{
  std::string msg_num;
  std::vector< uint8_t > data;
};

class AsyncFutureClient
{
 public:
  AsyncFutureClient(boost::asio::io_context& ctx, const std::string& host, const std::string& port)
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
                    std::print(stderr, "Connected to server.\n");
                    connected_ = true;
                    do_read_length();
                  }
                  else
                  {
                    // There are no more endpoints to try. Shut down the client.
                    stop();
                  }
                });
          }
        });
  }

  std::future< Response > send_request(const std::vector< uint8_t >& payload)
  {
    uint16_t num = msg_counter_.fetch_add(1, std::memory_order_relaxed) % std::numeric_limits< uint16_t >::max();
    std::string msg_num = std::format("{:05}", num);  // 5-digit ASCII message number

    // Build message body: msg_num + payload
    std::vector< uint8_t > body;
    body.insert(body.end(), msg_num.begin(), msg_num.end());
    body.insert(body.end(), payload.begin(), payload.end());

    // Prepend 4-byte ASCII length
    std::string len_str = std::format("{:04}", body.size());
    std::vector< uint8_t > full_msg;
    full_msg.insert(full_msg.end(), len_str.begin(), len_str.end());
    full_msg.insert(full_msg.end(), body.begin(), body.end());

    //===========================
    std::promise< Response > prom;
    auto fut = prom.get_future();
    {
      std::scoped_lock lock(map_mutex_);
      pending_.emplace(msg_num, std::move(prom));
    }
    //===========================

    size_t counter{4};
    while (!connected_ && --counter)
    {
      if (stopped_)
      {
        return fut;
      }
      std::print(stderr, "Client is not connected yet.\n");
      std::this_thread::sleep_for(250ms);  // NOLINT(misc-include-cleaner)
    }

    std::print("Send msg ({}): {}\n", msg_num, std::string(full_msg.begin(), full_msg.end()));

    boost::asio::async_write(socket_, boost::asio::buffer(full_msg),
        [this, msg_num](std::error_code ec, std::size_t /*n*/)
        {
          if (ec)
          {
            //===========================
            std::scoped_lock lock(map_mutex_);
            auto it = pending_.find(msg_num);
            if (it != pending_.end())
            {
              it->second.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
              pending_.erase(it);
            }
            //===========================
          }
        });

    return fut;
  }

  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
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

 private:
  void do_read_length()
  {
    boost::asio::async_read(socket_, boost::asio::buffer(len_buf_),
        [this](std::error_code ec, std::size_t /*n*/)
        {
          if (!ec)
          {
            std::string len_str(len_buf_.begin(), len_buf_.end());
            std::size_t msg_len = std::stoul(len_str, nullptr, 10);
            do_read_body(msg_len);
          }
          else
          {
            std::print(stderr, "Read length error: {} {}\n", __func__, ec.message());
            // XXX std::runtime_error(ec.message());
            // There are no more endpoints to try. Silently shut down the client.
            stop();
          }
        });
  }

  void do_read_body(std::size_t msg_len)
  {
    body_buf_.resize(msg_len);
    boost::asio::async_read(socket_, boost::asio::buffer(body_buf_),
        [this](std::error_code ec, std::size_t /*n*/)
        {
          if (!ec && body_buf_.size() >= 5)
          {
            std::string msg_num(body_buf_.begin(), body_buf_.begin() + 5);
            std::vector< uint8_t > payload(body_buf_.begin() + 5, body_buf_.end());
            handle_response(msg_num, std::move(payload));
            do_read_length();
          }
          else if (ec)
          {
            std::print(stderr, "Read body error: {} {}\n", __func__, ec.message());
            std::runtime_error(ec.message());
          }
        });
  }

  void handle_response(const std::string& msg_num, std::vector< uint8_t > data)
  {
    //===========================
    std::scoped_lock lock(map_mutex_);
    auto it = pending_.find(msg_num);
    if (it != pending_.end())
    {
      it->second.set_value(Response{msg_num, std::move(data)});
      pending_.erase(it);
    }
    //===========================
  }

  boost::asio::io_context& io_context_;
  tcp::resolver resolver_;
  tcp::socket socket_;
  std::array< char, 4 > len_buf_{};
  std::vector< uint8_t > body_buf_;
  std::atomic< uint16_t > msg_counter_{0};
  std::atomic< bool > connected_{false};
  std::atomic< bool > stopped_{false};

  //===========================
  std::mutex map_mutex_;
  std::map< std::string, std::promise< Response > > pending_;
  //===========================
};

int main(int argc, char* argv[])
{
  if (argc != 3)
  {
    std::print("Usage: {} <host> <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  std::string host = argv[1];
  std::string port = argv[2];

  try
  {
    boost::asio::io_context ctx;
    tcp::resolver resolver(ctx);
    AsyncFutureClient client(ctx, host, port);

    std::jthread io_thread([&ctx] { ctx.run(); });

    // Example request
    std::vector< uint8_t > payload{'H', 'e', 'l', 'l', 'o', ' ', 'T', 'e', 's', 't'};
    do
    {
      auto fut = client.send_request(payload);
      if (fut.wait_for(std::chrono::seconds(3)) == std::future_status::ready)
      {
        auto resp = fut.get();
        std::print("Received response ({}): {}\n", resp.msg_num, std::string(resp.data.begin(), resp.data.end()));
      }
      else
      {
        std::print(stderr, "Timeout waiting for response\n");
        break;
      }
    } while (true);

    ctx.stop();
  }
  catch (const std::exception& ex)
  {
    std::print(stderr, "Exception: {}\n", ex.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
