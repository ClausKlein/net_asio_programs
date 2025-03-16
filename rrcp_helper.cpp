#include "rrcp_helper.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

constexpr char ESC = 0x1B;
constexpr char REPLACE_LF = 0x01;
constexpr char REPLACE_CR = 0x02;
constexpr char REPLACE_ESC = 0x03;

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

      char const next = data[++i];
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
  for (char const c : data)
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
