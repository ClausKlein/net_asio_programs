#include <fmt/format.h>

#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "rrcp_helper.hpp"

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

constexpr size_t max_length = 65432;
constexpr auto timeout_duration = 1s;

class AsynchronousTCPClient : public std::enable_shared_from_this< AsynchronousTCPClient >
{
 public:
  AsynchronousTCPClient(boost::asio::io_context& io_context, const std::string& host, const std::string& port)
  : resolver_(io_context), socket_(io_context), timer_(io_context)
  {
    resolver_.async_resolve(host, port,
        [this](boost::system::error_code ec, tcp::resolver::results_type results)
        {
          if (!ec)
          {
            boost::asio::async_connect(socket_, results,
                [this](boost::system::error_code ec, const tcp::endpoint&)
                {
                  if (!ec)
                  {
                    fmt::print(stderr, "Connected to server.\n");
                    connected_ = true;
                    do_read();
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

  void write(const std::string& message)
  {
    while (!connected_)
    {
      if (stopped_)
      {
        return;
      }
      fmt::print(stderr, "Client is not connected yet.\n");
      std::this_thread::sleep_for(timeout_duration);  // NOLINT(misc-include-cleaner)
    }

    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            fmt::print(stderr, "Message sent.\n");
          }
          else
          {
            // There are no more endpoints to try. Shut down the client.
            stop();
          }
        });
  }

  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
  void stop()
  {
    boost::system::error_code ignored_error;
    socket_.close(ignored_error);
    timer_.cancel();
    connected_ = true;
    stopped_ = true;
  }

 private:
  void do_read()
  {
    if (stopped_)
    {
      return;
    }

    auto self(shared_from_this());

    if (!connected_)
    {
      timer_.expires_after(timeout_duration);
      timer_.async_wait(
          [this, self](const boost::system::error_code& ec)
          {
            if (!ec)
            {
              fmt::print(stderr, "Error: Read operation timed out.\n");
              stop();
            }
          });
    }

    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(data_), CR,
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          timer_.cancel();
          if (!ec)
          {
            std::string response = esc2char(data_.substr(1, length));  // NOTE: w/o LF!

            // NOTE: data_.erase(0, length); is used instead of data_.clear() because:

            // - Partial Data Handling: The async_read_until() function reads
            //   data up to the delimiter (CR) but doesn’t guarantee it consumes
            //   all the data in the socket.
            //   There might be extra data left in the buffer after the delimiter!

            // - Efficient Buffer Management: By erasing only the portion of the
            //   string that has been processed (length), we keep any remaining
            //   data intact for future reads instead of discarding it.
            data_.erase(0, length);

            fmt::print("Response is: {}\n", response);
            do_read();
          }
          else
          {
            // There are no more endpoints to try. Shut down the client.
            stop();
          }
        });
  }

  tcp::resolver resolver_;
  tcp::socket socket_;
  boost::asio::steady_timer timer_;
  std::string data_;
  bool connected_{false};
  bool stopped_{false};
};

auto main(int argc, char* argv[]) -> int
{
  try
  {
    if (argc != 3)
    {
      fmt::print(stderr, "Usage: async_tcp_echo_client <host> <port>\n");
      return EXIT_FAILURE;
    }

    boost::asio::io_context io_context;

    auto client = std::make_shared< AsynchronousTCPClient >(io_context, argv[1], argv[2]);

    std::thread io_thread([&io_context]() { io_context.run(); });

    for (std::string line; std::getline(std::cin, line); fmt::print(stderr, "Enter command: "))
    {
      const std::string::size_type sz = line.find("//");
      if ((sz != std::string::npos))
      {
        line.resize(sz);  // NOTE: w/o c++ comments
      }

      boost::trim_right(line);
      if (line.empty())
      {
        continue;
      }

      std::string command = char2esc(line);
      command.insert(0, 1, LF);
      command += CR;

      client->write(command);
    }
    std::this_thread::sleep_for(timeout_duration);  // NOLINT(misc-include-cleaner)

    client->stop();
    io_thread.join();
  }
  catch (std::exception& e)
  {
    fmt::print(stderr, "Exception: {}\n", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
