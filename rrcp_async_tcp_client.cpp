#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

constexpr char ESC = 0x1B;
constexpr char REPLACE_LF = 0x01;
constexpr char REPLACE_CR = 0x02;
constexpr char REPLACE_ESC = 0x03;

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

auto esc2char(std::string_view data) -> std::string
{
  std::string message;
  auto len = data.size();
  for (size_t i = 0; i < len; ++i)
  {
    char c = data[i];

    if (c == '\r')
    {
      return message;
    }

    if (c == ESC)
    {
      if (i == len - 1)
      {
        throw std::runtime_error("esc2char: Error - message ends with escape character!");
      }

      char next = data[++i];
      switch (next)
      {
      case REPLACE_LF:
        c = '\n';
        break;
      case REPLACE_CR:
        c = '\r';
        break;
      case REPLACE_ESC:
        c = ESC;
        break;
      default:
        throw std::runtime_error("esc2char: Error - unexpected ESC character!");
      }
    }

    message.push_back(c);
  }
  return message;
}

auto char2esc(std::string_view data) -> std::string
{
  std::string message;
  for (char c : data)
  {
    switch (c)
    {
    case '\n':
      message.push_back(ESC);
      message.push_back(REPLACE_LF);
      break;
    case '\r':
      message.push_back(ESC);
      message.push_back(REPLACE_CR);
      break;
    case ESC:
      message.push_back(ESC);
      message.push_back(REPLACE_ESC);
      break;
    default:
      message.push_back(c);
      break;
    }
  }
  return message;
}

class client : public std::enable_shared_from_this< client >
{
 public:
  client(boost::asio::io_context& io_context) : socket_(io_context), deadline_(io_context), heartbeat_timer_(io_context)
  {
    deadline_.expires_at(boost::asio::steady_timer::time_point::max());
  }

  void start(tcp::resolver::results_type endpoints)
  {
    boost::asio::async_connect(socket_, endpoints,
        [self = shared_from_this()](const boost::system::error_code& ec, const tcp::endpoint&)
        {
          if (!ec)
          {
            std::cout << "Connected to server.\n";
            self->read();
            self->start_heartbeat();
            self->check_deadline();
          }
          else
          {
            std::cerr << "Failed to connect: " << ec.message() << '\n';
          }
        });
  }

  void write()
  {
    std::string message;
    while (std::getline(std::cin, message))
    {
      if (stopped_) return;

      message = '\n' + char2esc(message) + '\r';
      boost::asio::async_write(socket_, boost::asio::buffer(message),
          [self = shared_from_this()](const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              std::cerr << "Error sending message: " << ec.message() << '\n';
              self->stop();
            }
          });
      deadline_.expires_after(3s);
    }
  }

  void read()
  {
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(input_buffer_), '\r',
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t length)
        {
          if (!ec)
          {
            std::string line = esc2char(self->input_buffer_.substr(0, length - 1));
            self->input_buffer_.erase(0, length);
            // XXX if (line != "GPing")
            {
              std::cout << "Server: " << line << '\n';
            }
            self->read();
            self->deadline_.expires_after(3s);
          }
          else
          {
            std::cerr << "Error reading message: " << ec.message() << '\n';
            self->stop();
          }
        });
  }

  void start_heartbeat()
  {
    if (stopped_) return;

    std::string heartbeat_message{'\n' + char2esc("GPing") + '\r'};
    std::cerr << "Send heartbeat: " << heartbeat_message << '\n';
    boost::asio::async_write(socket_, boost::asio::buffer(heartbeat_message),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t)
        {
          if (!ec)
          {
            self->heartbeat_timer_.expires_after(10s);
            self->heartbeat_timer_.async_wait([self](const boost::system::error_code&) { self->start_heartbeat(); });
          }
          else
          {
            std::cerr << "Error sending heartbeat: " << ec.message() << '\n';
            self->stop();
          }
        });
  }

  void check_deadline()
  {
    if (stopped_) return;

    if (deadline_.expiry() <= boost::asio::steady_timer::clock_type::now())
    {
      std::cerr << "No response from server, disconnecting...\n";
      stop();
      return;
    }

    deadline_.async_wait([self = shared_from_this()](const boost::system::error_code&) { self->check_deadline(); });
  }

  void stop()
  {
    std::cerr << "stop called, disconnecting...\n";
    stopped_ = true;
    boost::system::error_code ec;
    socket_.close(ec);
    heartbeat_timer_.cancel();
    deadline_.cancel();
  }

 private:
  tcp::socket socket_;
  boost::asio::steady_timer deadline_, heartbeat_timer_;
  std::string input_buffer_;
  bool stopped_{false};
};

int main(int argc, char* argv[])
{
  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;
  tcp::resolver resolver(io_context);

  auto c = std::make_shared< client >(io_context);
  c->start(resolver.resolve(argv[1], argv[2]));

  std::thread io_thread([&io_context]() { io_context.run(); });
  c->write();
  c->stop();
  io_thread.join();

  return 0;
}
