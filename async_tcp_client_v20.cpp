/***
 * async_tcp_client_v20.cpp
 * ~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * Moderniced from Claus Klein and ChatGPT
 ***/

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>  // NOLINT(misc-include-cleaner)
#include <cstddef>
#include <exception>
#include <format>
#include <iostream>  // For std::getline
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <utility>

using boost::asio::steady_timer;
using boost::asio::ip::tcp;
using namespace std::chrono_literals;

class client : public std::enable_shared_from_this< client >
{
 public:
  explicit client(boost::asio::io_context& io_context)
  : socket_(io_context), deadline_(io_context), heartbeat_timer_(io_context)
  {
  }

  // Called by the user of the client class to initiate the connection
  // process. The endpoints will have been obtained using a tcp::resolver.
  void start(tcp::resolver::results_type endpoints)
  {
    // Start the connect actor.
    endpoints_ = std::move(endpoints);
    start_connect(endpoints_.begin());

    // Start the deadline actor. You will note that we're not setting any
    // particular deadline here. Instead, the connect and input actors will
    // update the deadline prior to each asynchronous operation.
    deadline_.async_wait([this](const boost::system::error_code& /*e*/) { check_deadline(); });
  }

  void write()
  {
    auto self(shared_from_this());

    for (std::string message; !stopped_ && std::getline(std::cin, message); std::print("Enter message to send: "))
    {
      if (message.empty())
      {
        continue;
      }

      if (message.back() != '\n')
      {
        message += '\n';
      }

      std::print(stderr, "Sending: {}\n", message);

      // Start an asynchronous operation to send the message.
      boost::asio::async_write(socket_, boost::asio::buffer(message),
          [this, self](const boost::system::error_code& error, std::size_t) { handle_write(error); });
    }
  }

  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
  void stop()
  {
    stopped_ = true;
    boost::system::error_code ignored_error;
    socket_.close(ignored_error);
    deadline_.cancel();
    heartbeat_timer_.cancel();
  }

 private:
  void start_connect(const tcp::resolver::results_type::iterator& endpoint_iter)
  {
    if (endpoint_iter != endpoints_.end())
    {
      std::print("Trying {}:{}...\n", endpoint_iter->endpoint().address().to_string(), endpoint_iter->endpoint().port());

      // Set a deadline for the connect operation.
      deadline_.expires_after(6s);

      // Start the asynchronous connect operation.
      socket_.async_connect(endpoint_iter->endpoint(),
          [this, endpoint_iter](const boost::system::error_code& error) { handle_connect(error, endpoint_iter); });
    }
    else
    {
      // There are no more endpoints to try. Shut down the client.
      stop();
    }
  }

  void handle_connect(const boost::system::error_code& error, tcp::resolver::results_type::iterator endpoint_iter)
  {
    if (stopped_)
    {
      return;
    }

    // The async_connect() function automatically opens the socket at the
    // start of the asynchronous operation. If the socket is closed at this
    // time then the timeout handler must have run first.
    if (!socket_.is_open())
    {
      std::print(stderr, "Connect timed out\n");

      // Try the next available endpoint.
      start_connect(++endpoint_iter);
    }

    // Check if the connect operation failed before the deadline expired.
    else if (error)
    {
      std::print(stderr, "Error on Connect: {}\n", error.message());

      // We need to close the socket used in the previous connection
      // attempt before starting a new one.
      socket_.close();

      // Try the next available endpoint.
      start_connect(++endpoint_iter);
    }

    // Otherwise we have successfully established a connection.
    else
    {
      std::print(
          "Connected to {}:{}\n", endpoint_iter->endpoint().address().to_string(), endpoint_iter->endpoint().port());

      // Start the input actor.
      start_read();

      // Start the heartbeat actor.
      start_write();
    }
  }

  void start_read()
  {
    if (stopped_)
    {
      return;
    }

    // Set a deadline for the read operation.
    deadline_.expires_after(30s);

    auto self(shared_from_this());

    // Start an asynchronous operation to read a newline-delimited message.
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(input_buffer_), '\n',
        [this, self](const boost::system::error_code& error, std::size_t n) { handle_read(error, n); });
  }

  void handle_read(const boost::system::error_code& error, std::size_t n)
  {
    if (stopped_)
    {
      return;
    }

    if (!error)
    {
      // Extract the newline-delimited message from the buffer.
      std::string line = input_buffer_.substr(0, n - 1);  // NOTE: w/o '\n'
      input_buffer_.erase(0, n);

      // Empty messages are heartbeats and so ignored.
      if (line.empty())
      {
        std::print(stderr, "Received: {}\n", "hartbeat");
      }
      else
      {
        std::print("Received: {}\n", line);
      }

      start_read();
    }
    else
    {
      std::print(stderr, "Error on receive: {}\n", error.message());

      stop();
    }
  }

  void start_write()
  {
    if (stopped_)
    {
      return;
    }

    std::string message{'\n'};
    std::print(stderr, "Sending: {}\n", "hartbeat");

    auto self(shared_from_this());

    // Start an asynchronous operation to send a heartbeat message.
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self](const boost::system::error_code& error, std::size_t) { handle_write(error); });
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (stopped_)
    {
      return;
    }

    if (!error)
    {
      if (heartbeat_timer_.expiry() <= steady_timer::clock_type::now())
      {
        std::print(stderr, "Waiting for next to send: {}\n", "hartbeat");
      }

      // Wait 10 seconds before sending the next heartbeat.
      heartbeat_timer_.cancel();
      heartbeat_timer_.expires_after(10s);
      heartbeat_timer_.async_wait([this](const boost::system::error_code& /*e*/) { start_write(); });
    }
    else
    {
      std::print(stderr, "Error on sending heartbeat: {}\n", error.message());

      stop();
    }
  }

  void check_deadline()
  {
    if (stopped_)
    {
      return;
    }

    // Check whether the deadline has passed. We compare the deadline
    // against the current time since a new asynchronous operation may have
    // moved the deadline before this actor had a chance to run.
    if (deadline_.expiry() <= steady_timer::clock_type::now())
    {
      // The deadline has passed. The socket is closed so that any
      // outstanding asynchronous operations are cancelled.
      socket_.close();

      // There is no longer an active deadline. The expiry is set to the
      // maximum time point so that the actor takes no action until a new
      // deadline is set.
      deadline_.expires_at(steady_timer::time_point::max());
    }

    auto self(shared_from_this());

    // Put the actor back to sleep.
    deadline_.async_wait([this, self](const boost::system::error_code& /*e*/) { check_deadline(); });
  }

  bool stopped_{false};
  tcp::resolver::results_type endpoints_;
  tcp::socket socket_;
  std::string input_buffer_;
  steady_timer deadline_;
  steady_timer heartbeat_timer_;
};

auto main(int argc, char* argv[]) -> int
{
  try
  {
    if (argc != 3)
    {
      std::print("Usage: {} <host> <port>\n", *argv);
      return EXIT_FAILURE;
    }

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);

    auto c = std::make_shared< client >(io_context);

    c->start(resolver.resolve(argv[1], argv[2]));

    std::thread io_thread([&io_context]() { io_context.run(); });

    c->write();

    c->stop();
    io_thread.join();
  }
  catch (std::exception& e)
  {
    std::print("Exception: {}\n", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
