#include "Base64.hpp"

#define USE_BOOST_BEAST
#ifdef USE_BOOST_BEAST
// with homebrew /usr/local/include/boost/beast/core/detail/base64.hpp
// and its impl. /usr/local/include/boost/beast/core/detail/base64.ipp
// /Users/clausklein/.local/include/boost/beast/core/detail/base64.hpp
#include <boost/beast/core/detail/base64.hpp>
#endif

#ifdef USE_BOOST_BEAST

#include <algorithm>
#include <cctype>

#ifdef __cpp_lib_ranges
#include <ranges>
#endif

namespace
{

using boost::beast::detail::base64::decode;
using boost::beast::detail::base64::decoded_size;
using boost::beast::detail::base64::encode;
using boost::beast::detail::base64::encoded_size;

class base64
{
#ifndef __cpp_lib_ranges
  // Function to remove all whitespace characters from a std::string (C++17)
  static auto remove_whitespace(std::string_view input) -> std::string
  {
    std::string result{input.data(), input.length()};
    result.erase(
        std::remove_if(result.begin(), result.end(), [](unsigned char c) { return std::isspace(c); }), result.end());
    return result;
  }
#else
  // Function to remove all whitespace characters from a std::string_view (C++20)
  static auto remove_whitespace(std::string_view input) -> std::string
  {
    auto filtered = input | std::views::filter([](unsigned char c) { return !std::isspace(c); });
    return {filtered.begin(), filtered.end()};
  }
#endif

  static auto base64_encode(std::uint8_t const* data, std::size_t len) -> std::string
  {
    std::string dest;
    dest.resize(encoded_size(len));
    dest.resize(encode(dest.data(), data, len));
    return dest;
  }

 public:
  static auto base64_encode(std::string_view s) -> std::string
  {
    return base64_encode(reinterpret_cast< std::uint8_t const* >(s.data()), s.size());
  }

  static auto base64_decode(std::string_view data) -> std::string
  {
    std::string dest;
    dest.resize(decoded_size(data.size()));

    // TODO(CK): remove first at least all "\n\r" or better all non printable chars!
    std::string striped = remove_whitespace(data);
    auto const result = decode(dest.data(), striped.data(), striped.size());
    dest.resize(result.first);
    return dest;
  }
};

}  // namespace

#else

// XXX #include <cassert>
#include <stdexcept>

#endif

namespace RRCP::Common
{

auto Base64::encode(std::string_view data) -> std::string
{
  if (data.empty())
  {
    return {};
  }

#ifdef USE_BOOST_BEAST

  return base64::base64_encode(data);

#else

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

    encoded.append(1, BaseChars_[i1]);
    encoded.append(1, BaseChars_[i2]);
    encoded.append(1, BaseChars_[i3]);
    encoded.append(1, BaseChars_[i4]);
    if (encodeWithLinebreak_)
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

    encoded.append(1, BaseChars_[i1]);
    encoded.append(1, BaseChars_[i2]);
    encoded.append(2, '=');
  }
  else if ((data.size() % 3) == 2)
  {
    // Two octets remaining.
    auto i1 = std::uint8_t((data[data.size() - 2] & 0xfc) >> 2U);
    auto i2 = std::uint8_t(((data[data.size() - 2] & 0x03) << 4U) | ((data[data.size() - 1] & 0xf0) >> 4U));
    auto i3 = std::uint8_t(((data[data.size() - 1] & 0x0f) << 2U));
    // XXX assert((i1 < 64) && (i2 < 64) && (i3 < 64));

    encoded.append(1, BaseChars_[i1]);
    encoded.append(1, BaseChars_[i2]);
    encoded.append(1, BaseChars_[i3]);
    encoded.append(1, '=');
  }

  return encoded;

#endif
}

auto Base64::decode(std::string_view in) -> std::string
{
#ifdef USE_BOOST_BEAST

  return base64::base64_decode(in);

#else

  std::string decoded;
  std::string fourChars;

  // Iterate over the input string
  for (const char i : in)
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
        decoded += static_cast< char >(((i2 << 4U) & 0xf0) | (i3 >> 2U));
      }
      if (i4 != 0)
      {
        decoded += static_cast< char >(((i3 << 6U) & 0xc0) | i4);
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

#endif
}

#ifndef USE_BOOST_BEAST
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
#endif

}  // namespace RRCP::Common
