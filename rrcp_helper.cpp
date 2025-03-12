#include "rrcp_helper.hpp"

#include <cstddef>
#include <iostream>
#include <string>

constexpr const char ESC = 0x1B;
constexpr const char REPLACE_LF = 0x01;
constexpr const char REPLACE_CR = 0x02;
constexpr const char REPLACE_ESC = 0x03;

constexpr const char LF = 0x0A;
constexpr const char CR = 0x0D;

// constexpr const char SP = 0x20;
// constexpr const char DOT = 0x2E;
// constexpr const char SEMICOLON = 0x3B;
// constexpr const char COMMA = 0x2C;
// constexpr const char SHARP = 0x23;
// constexpr const char DQUOTE = '\"';
// constexpr const char BACKSLASH = '\\';
// constexpr const char COLON = 0x3A;

auto esc2char(const std::string& data) -> std::string
{
  std::string message;
  size_t const len = data.size();
  char c = 0;
  unsigned int i = 0;
  while (i < len)
  {
    // get next char
    c = data[i];

    // end mark found, the message is complete, the
    // CR is not needed anymore
    if (c == CR)
    {
      return message;
    }

    // An escape is found thus we want to
    // replace the escape sequence
    if (c == ESC)
    {
      // On the ESC should follow an replacement character
      // REPLACE_LF... REPLACE_ESC
      if (i != (len - 1))
      {
        // get next character
        i++;

        c = data[i];
        if (REPLACE_LF == c)
        {
          c = static_cast< char >(LF);
        }
        else if (REPLACE_CR == c)
        {
          c = static_cast< char >(CR);
        }
        else if (REPLACE_ESC == c)
        {
          c = static_cast< char >(ESC);
        }
        else
        {
          std::cout << "esc2char: Error contains unexpected ESC character!\n";
          return "";
        }
      }
      else
      {
        std::cout << "esc2char: Error message ends with escape character!\n";
        return "";
      }
    }

    // append current character
    message += c;
    // continue with next character
    i++;
  }
  return message;
}

auto char2esc(const std::string& data) -> std::string
{
  std::string message;
  size_t const len = data.size();
  char c = 0;
  unsigned int i = 0;

  while (i < len)
  {
    // get next char
    c = data[i++];
    // and replace CR and LF and the ESC itself
    switch (c)
    {
    case LF:
    {
      message += static_cast< char >(ESC);
      message += static_cast< char >(REPLACE_LF);
    };
    break;
    case CR:
    {
      message += static_cast< char >(ESC);
      message += static_cast< char >(REPLACE_CR);
    };
    break;

    case ESC:
    {
      message += static_cast< char >(ESC);
      message += static_cast< char >(REPLACE_ESC);
    };
    break;
    default:
    {
      message += c;
    }
    break;
    }
  }
  return message;
}
