#include "Base64.hpp"

// XXX #include <cassert>
#include <cstring>
#include <stdexcept>

namespace RRCP::Common
{

auto Base64::encode(std::string_view data) const -> std::string
{
  if (data.empty())
  {
    throw std::invalid_argument("Invalid input data");
  }

  std::string encoded;
  size_t linelen = 0;

  // Encode all complete 3 octet blocks
  for (size_t i = 0; i < (data.size() / 3); ++i)
  {
    size_t const pos = 3 * i;
    auto i1 = std::uint8_t((data[pos] & 0xfc) >> 2U);
    auto i2 = std::uint8_t(((data[pos] & 0x03) << 4U) | ((data[pos + 1] & 0xf0) >> 4U));
    auto i3 = std::uint8_t(((data[pos + 1] & 0x0f) << 2U) | ((data[pos + 2] & 0xfc) >> 6U));
    auto i4 = std::uint8_t((data[pos + 2] & 0x3f));
    // XXX assert((i1 < 64) && (i2 < 64) && (i3 < 64) && (i4 < 64));

    encoded.append(1, m_BaseChars[i1]);
    encoded.append(1, m_BaseChars[i2]);
    encoded.append(1, m_BaseChars[i3]);
    encoded.append(1, m_BaseChars[i4]);
    if (m_encodeWithLinebreak)
    {
      linelen += 4;
      if (linelen >= 76)
      {
        linelen = 0;
        encoded.append("\n");
      }
    }
  }

  // Handle remaining octets
  if ((data.size() % 3) == 1)
  {
    // One octet remaining.
    auto i1 = std::uint8_t((data[data.size() - 1] & 0xfc) >> 2U);
    auto i2 = std::uint8_t(((data[data.size() - 1] & 0x03) << 4U));
    // XXX assert((i1 < 64) && (i2 < 64));

    encoded.append(1, m_BaseChars[i1]);
    encoded.append(1, m_BaseChars[i2]);
    encoded.append(2, '=');
  }
  else if ((data.size() % 3) == 2)
  {
    // Two octets remaining.
    auto i1 = std::uint8_t((data[data.size() - 2] & 0xfc) >> 2U);
    auto i2 = std::uint8_t(((data[data.size() - 2] & 0x03) << 4U) | ((data[data.size() - 1] & 0xf0) >> 4U));
    auto i3 = std::uint8_t(((data[data.size() - 1] & 0x0f) << 2U));
    // XXX assert((i1 < 64) && (i2 < 64) && (i3 < 64));

    encoded.append(1, m_BaseChars[i1]);
    encoded.append(1, m_BaseChars[i2]);
    encoded.append(1, m_BaseChars[i3]);
    encoded.append(1, '=');
  }

  return encoded;
}

auto Base64::decode(std::string_view in) -> std::string
{
  std::string decoded;
  std::string fourChars;

  // Iterate over the input string
  for (char i : in)
  {
    if (isBase64Char(i) || (i == '='))
    {
      fourChars += i;
    }
    else if ((i != '\n') && (i != '\r'))
    {
      // Invalid character, throw an exception
      throw std::invalid_argument("Invalid Base64 character");
    }

    // If we have four characters, decode them
    if (fourChars.size() == 4)
    {
      std::uint8_t const i1 = base64CharValue(fourChars[0]);
      std::uint8_t const i2 = base64CharValue(fourChars[1]);
      std::uint8_t const i3 = (fourChars[2] == '=') ? 0 : base64CharValue(fourChars[2]);
      std::uint8_t const i4 = (fourChars[3] == '=') ? 0 : base64CharValue(fourChars[3]);

      decoded += static_cast< char >((i1 << 2U) | (i2 >> 4U));
      if (i3 != 0)
      {
        decoded += static_cast< char >(((i2 << 4U) /* & 0xf0 */) | (i3 >> 2U));
      }
      if (i4 != 0)
      {
        decoded += static_cast< char >(((i3 << 6U) /* & 0xc0 */) | i4);
      }

      fourChars.clear();
    }
  }

  // Check if there are any remaining characters
  if (!fourChars.empty())
  {
    throw std::invalid_argument("Invalid Base64 string");
  }

  return decoded;
}

auto Base64::isBase64Char(char c) const -> bool
{
  return (
      ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '+') || (c == '/'));
}

auto Base64::base64CharValue(char c) const -> std::uint8_t
{
  if ((c >= 'A') && (c <= 'Z'))
  {
    return std::uint8_t(c - 'A');
  }
  if ((c >= 'a') && (c <= 'z'))
  {
    return std::uint8_t(c - 'a' + 26U);
  }
  if ((c >= '0') && (c <= '9'))
  {
    return std::uint8_t(c - '0' + 52U);
  }
  if (c == '+')
  {
    return 62U;
  }
  if (c == '/')
  {
    return 63U;
  }
  throw std::invalid_argument("Invalid Base64 character");
}

}  // namespace RRCP::Common
