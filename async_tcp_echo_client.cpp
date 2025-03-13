#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <print>
#include <string>
#include <thread>

#include "rrcp_helper.hpp"

using boost::asio::ip::tcp;

constexpr size_t max_length = 1024;

class AsynchronousTCPClient : public std::enable_shared_from_this< AsynchronousTCPClient >
{
 public:
  AsynchronousTCPClient(boost::asio::io_context& io_context, const std::string& host, const std::string& port)
  : resolver_(io_context), socket_(io_context)
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
                    std::print("Connected to server.\nEnter command: ");
                    do_read();
                  }
                });
          }
        });
  }

  void write(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            std::print("Message sent.\nEnter command: ");
          }
        });
  }

 private:
  void do_read()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(data_), CR,
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::string response = esc2char(data_.substr(0, length));
            // NOTE: data_.erase(0, length); is used instead of data_.clear()
            // because:

            // - Partial Data Handling: The async_read_until() function reads
            // data up to the delimiter (CR) but
            //   doesn’t guarantee it consumes all the data in the socket. There
            //   might be extra data left in the buffer after the delimiter.

            //- Efficient Buffer Management: By erasing only the portion of the
            // string that has been processed
            //  (length), we keep any remaining data intact for future reads
            //  instead of discarding it.
            data_.erase(0, length);

            std::print("Response is: {}\n", response);
            do_read();
          }
        });
  }

  tcp::resolver resolver_;
  tcp::socket socket_;
  std::string data_;
};

auto main(int argc, char* argv[]) -> int
{
  try
  {
    if (argc != 3)
    {
      std::print("Usage: async_tcp_client <host> <port>\n");
      return EXIT_FAILURE;
    }

    boost::asio::io_context io_context;

    auto client = std::make_shared< AsynchronousTCPClient >(io_context, argv[1], argv[2]);

    std::thread io_thread([&io_context]() { io_context.run(); });

    std::string line;
    while (std::getline(std::cin, line))
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

      std::string command = char2esc(line);
      command.insert(0, 1, LF);
      command += CR;

      client->write(command);
    }

    io_thread.join();
  }
  catch (std::exception& e)
  {
    std::print("Exception: {}\n", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
