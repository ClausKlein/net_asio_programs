/***
Additionally, you may want to consider adding more test cases to cover edge cases, such as:

Null or empty input data
Input data with invalid characters (e.g., non-ASCII characters)
Input data with padding errors (e.g., incorrect number of padding characters)
Input data with encoding errors (e.g., incorrect encoding scheme)

By covering these edge cases, you can ensure that your Base64 class is robust and reliable.
***/

#include "Base64.hpp"

#include <gtest/gtest.h>

#include <print>
#include <random>
#include <string>

using namespace std::string_literals;

// TODO: using RRCP::Common::Base64;
#define TEST_RANDOM_VALUES

namespace
{
struct testpattern_t
{
  const char *bin;
  const char *encoded;
} testpattern[] = {{"", ""}, {" ", "IA=="}, {"  ", "ICA="}, {"   ", "ICAg"}, {"    ", "ICAgIA=="}, {"     ", "ICAgICA="},
    {"      ", "ICAgICAg"}, {"       ", "ICAgICAgIA=="}, {"U", "VQ=="}, {"UU", "VVU="}, {"UUU", "VVVV"},
    {"UUUU", "VVVVVQ=="}, {"UUUUU", "VVVVVVU="}, {"UUUUUU", "VVVVVVVV"}, {"UUUUUUU", "VVVVVVVVVQ=="},
    {"Franz jagt in einem total verwahrlosten Taxi quer durch Bayern",
        "RnJhbnogamFndCBpbiBlaW5lbSB0b3RhbCB2ZXJ3YWhybG9zdGVuIFRheGkgcXVlciBkdXJjaCBCYXllcm4="},
    {nullptr, nullptr}};
}  // namespace

TEST(Base64Test, encoding)
{
  RRCP::Common::Base64 base64;
  base64.setLineBreak(false);

  size_t i = 1;
  while (testpattern[i].bin != nullptr)
  {
    const std::string result = base64.encode(testpattern[i].bin);
    const std::string encoded{testpattern[i].encoded};
    EXPECT_EQ(encoded, result);
    ++i;
  }
}

TEST(Base64Test, decoding)
{
  RRCP::Common::Base64 base64;

  size_t i = 0;
  while (testpattern[i].bin != nullptr)
  {
    const std::string result = base64.decode(testpattern[i].encoded);
    const std::string bin{testpattern[i].bin};
    EXPECT_EQ(bin, result);
    ++i;
  }
}

TEST(Base64Test, EmptyString)
{
  RRCP::Common::Base64 base64;
  std::string const original;
  EXPECT_THROW({ auto decoded = base64.encode(original); }, std::invalid_argument);
  // XXX std::string const decoded = base64.decode(encoded);
  // XXX EXPECT_EQ(original, decoded);
}

TEST(Base64Test, ShortString1)
{
  RRCP::Common::Base64 base64;
  std::string const original = "A";
  std::string const encoded = base64.encode(original);
  // std::println("{}:\t{}", original, encoded);
  EXPECT_EQ(encoded, "QQ==");

  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, ShortString2)
{
  RRCP::Common::Base64 base64;
  std::string const original = "AA";
  std::string const encoded = base64.encode(original);
  // std::println("{}:\t{}", original, encoded);
  EXPECT_EQ(encoded, "QUE=");

  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, ShortString3)
{
  RRCP::Common::Base64 base64;
  std::string const original = "AAA";
  std::string const encoded = base64.encode(original);
  // std::println("{}:\t{}", original, encoded);
  EXPECT_EQ(encoded, "QUFB");

  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, DecodeMarker)
{
  RRCP::Common::Base64 base64;
  EXPECT_ANY_THROW({ (void)base64.decode("====").empty(); });
  EXPECT_ANY_THROW({ (void)base64.decode("===").empty(); });
  EXPECT_ANY_THROW({ (void)base64.decode("==").empty(); });
  EXPECT_ANY_THROW({ (void)base64.decode("=").empty(); });
  EXPECT_ANY_THROW({ (void)base64.decode("\t").empty(); });

  EXPECT_NO_THROW({ (void)base64.decode("").empty(); });
}

TEST(Base64Test, MediumString)
{
  RRCP::Common::Base64 base64;
  std::string const original = "This is a medium length string.";
  std::string const encoded = base64.encode(original);
  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, LongString)
{
  RRCP::Common::Base64 base64;
  base64.setLineBreak(true);

  std::string const original = "This is not a really long string, but also that should be encoded and decoded correctly.";
  std::string const encoded = base64.encode(original);
  std::string const decoded = base64.decode(encoded);
  std::println("{}:\n{}", original, encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, FoxString)
{
  RRCP::Common::Base64 base64;
  std::string const original = "The quick brown fox jumped over the lazy dogs.";
  std::string const encoded = base64.encode(original);
  EXPECT_EQ(encoded, "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2dzLg==");
  // std::println("{}:\t{}", original, encoded);

  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, BinaryData)
{
  RRCP::Common::Base64 base64;
  std::string const original = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"s;
  std::string const encoded = base64.encode(original);
  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

TEST(Base64Test, NonAsciiString)
{
  RRCP::Common::Base64 base64;
  std::string const original = "\xFC@NOs[\xFEVJ\t@\x80\v\xD0\xAA\xF5";
  std::string const encoded = base64.encode(original);
  std::string const decoded = base64.decode(encoded);
  EXPECT_EQ(original, decoded);
}

#ifdef TEST_RANDOM_VALUES
TEST(Base64Test, RandomBinaryData)
{
  std::random_device rd;  // a seed source for the random number engine
  std::mt19937 gen(rd());  // mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<> distrib(0, 255);

  RRCP::Common::Base64 base64;
  base64.setLineBreak(true);

  for (size_t i = 0; i < 5; ++i)
  {
    const std::string::size_type new_cap{64u + i};
    std::string original;
    original.reserve(new_cap);
    for (size_t j = 0; j < new_cap; ++j)
    {
      original += static_cast< char >(distrib(gen) % 256);
    }
    std::string const encoded = base64.encode(original);
    std::string const decoded = base64.decode(encoded);
    EXPECT_EQ(original, decoded);
  }
}
#endif

auto main(int argc, char **argv) -> int
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
