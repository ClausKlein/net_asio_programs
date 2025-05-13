//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Moderniced from Claus Klein and ChatGPT

#include <array>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#ifdef TARGET_CODE_COVERAGE
// Forward declaration of flush api
// extern "C" {
extern void __gcov_flush();
// }
#endif

using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this< session >
{
  static constexpr size_t MAX_LENGTH{1024};

 public:
  explicit session(tcp::socket socket) : socket_(std::move(socket)) {}

  void start() { do_read(); }

 private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_.data(), MAX_LENGTH),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            do_write(length);
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_.data(), length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            do_read();
          }
          else
          {
            socket_.close();
          }
        });
  }

  tcp::socket socket_;
  std::array< char, MAX_LENGTH > data_{};
};

class server
{
 public:
  server(boost::asio::io_context& io_context, short port)
  : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), socket_(io_context), signals_(io_context)
  {
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    signals_.add(SIGINT);
    signals_.add(SIGTERM);

#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif  // defined(SIGQUIT)

    do_await_stop();

    do_accept();
  }

 private:
  void do_accept()
  {
    acceptor_.async_accept(socket_,
        [this](boost::system::error_code ec)
        {
          if (!ec)
          {
            std::make_shared< session >(std::move(socket_))->start();
            do_accept();
          }
          else
          {
            acceptor_.close();
            socket_.close();
          }
        });
  }

  // Signal handler definition which flushes profiling data
  void do_await_stop()
  {
    signals_.async_wait(
        [this](std::error_code ec, int signo)
        {
          std::cerr << "Signal handler called for " << signo << "\n";
          if (!ec)
          {
            // The server is stopped by cancelling all outstanding asynchronous
            // operations. Once all operations have finished the io_context::run()
            // call will exit.
            acceptor_.close();
            socket_.close();
          }
          else
          {
            acceptor_.close();
            socket_.close();

#ifdef TARGET_CODE_COVERAGE
            __gcov_flush();
#endif

            exit(EXIT_FAILURE);
          }
        });
  }

  tcp::acceptor acceptor_;
  tcp::socket socket_;
  boost::asio::signal_set signals_;
};

auto main(int argc, char* argv[]) -> int
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return EXIT_FAILURE;
    }

    boost::asio::io_context io_context;

    server const serv(io_context, std::strtol(argv[1], nullptr, 10));

    io_context.run();
    std::cout << "io_service.run complete, shutdown successful\n";
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
