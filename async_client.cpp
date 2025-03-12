#include <boost/asio.hpp>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>

const auto noop = std::bind([] {});
const std::string delimiter{"\r\n\r\n"};

boost::asio::io_context io_context;
boost::asio::ip::tcp::acceptor acceptor(
    io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
boost::asio::ip::tcp::socket socket1(io_context);
boost::asio::ip::tcp::socket socket2(io_context);
boost::asio::streambuf streambuf;

// void do_read();

void handle_read(boost::system::error_code, std::size_t xfer)
{
  assert(streambuf.size() >= xfer);

  std::string const command{buffers_begin(streambuf.data()),
      buffers_begin(streambuf.data()) + xfer - delimiter.length()};

  streambuf.consume(xfer);

  // XXX assert(command == "cmd1");
  std::cout << "received command: " << command << "\n"
            << "streambuf contains " << streambuf.size() << " bytes.\n";

  boost::asio::async_read_until(socket2, streambuf, delimiter, handle_read);
}

auto main() -> int
{
  acceptor.async_accept(socket1, noop);
  socket2.async_connect(acceptor.local_endpoint(), noop);
  io_context.run();
  io_context.restart();

  boost::asio::write(socket1, boost::asio::buffer("cmd1" + delimiter));
  boost::asio::write(socket1, boost::asio::buffer("cmd2" + delimiter));
  boost::asio::async_read_until(socket2, streambuf, delimiter, handle_read);

  io_context.run();
}
