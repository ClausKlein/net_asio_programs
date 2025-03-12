//
// timer.cpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>

void print(boost::asio::steady_timer* t, int* count)
{
  if (*count < 5)
  {
    std::cout << *count << '\n';
    ++(*count);

    t->expires_at(t->expiry() + boost::asio::chrono::seconds(1));
    t->async_wait([t, count](const boost::system::error_code& /*e*/)
        { print(t, count); });
  }
}

auto main() -> int
{
  boost::asio::io_context io;

  int count = 0;
  boost::asio::steady_timer t(io, boost::asio::chrono::seconds(1));
  t.async_wait(
      [capture0 = &t, capture1 = &count](const boost::system::error_code& /*e*/)
      { print(capture0, capture1); });

  io.run();

  std::cout << "Final count is " << count << '\n';

  return 0;
}
