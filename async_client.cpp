#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>

namespace
{

const auto noop = std::bind([] {});  // NOLINT(modernize-avoid-bind) NOTE: deprecated too! CK
const std::string delimiter{"\r\n\r\n"};

boost::asio::io_context io_context;
boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
boost::asio::ip::tcp::socket socket1(io_context);
boost::asio::ip::tcp::socket socket2(io_context);

std::string input_buffer_;
auto streambuf = boost::asio::dynamic_buffer(input_buffer_);
;

// void do_read();

void handle_read(boost::system::error_code /*unused*/, std::size_t xfer)
{
  assert(streambuf.size() >= xfer);

  std::string const command{input_buffer_.data(), xfer - delimiter.length()};

  streambuf.consume(xfer);

  std::cout << "received command: " << command << "\n"
            << "streambuf contains " << streambuf.size() << " bytes.\n";

  if (command == "cmd1")
  {
    boost::asio::async_read_until(socket2, streambuf, delimiter, handle_read);
  }
}

}  // namespace

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
