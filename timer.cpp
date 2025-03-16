//
// timer4/timer.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <functional>
#include <iostream>

class printer
{
  static constexpr int kMaxCount{5};

 public:
  explicit printer(boost::asio::io_context& io) : timer_(io, boost::asio::chrono::seconds(1))
  {
    // cpp11: timer_.async_wait(std::bind(&printer::print, this));
    timer_.async_wait([this](const boost::system::error_code& /*ec*/) { print(); });
  }

  ~printer() { std::cout << "Final count is " << count_ << '\n'; }

  void print()
  {
    if (count_ < kMaxCount)
    {
      std::cout << count_ << '\n';
      ++count_;

      timer_.expires_at(timer_.expiry() + boost::asio::chrono::seconds(1));
      // cpp11: timer_.async_wait(std::bind(&printer::print, this));
      timer_.async_wait([this](const boost::system::error_code& /*ec*/) { print(); });
    }
  }

 private:
  boost::asio::steady_timer timer_;
  int count_{};
};

auto main() -> int
{
  try
  {
    boost::asio::io_context io;
    auto p = std::make_unique< printer >(io);
    io.run();
  }
  catch (const std::exception& e)
  {
    std::print("Error: {}\n", e.what());
    return 1;
  }

  return 0;
}
