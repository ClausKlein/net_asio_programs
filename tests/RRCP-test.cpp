#include <boost/ut.hpp>  // import boost.ut;
#include <iomanip>  // use std::quoted
#include <sstream>
#include <string>
#include <string_view>

#include "rrcp_helper.hpp"

namespace ut = boost::ut;

ut::suite errors = []
{
  using namespace ut;
  using namespace std::literals;

  "find_response_msg"_test = []
  {
    constexpr std::string_view expected{"gGoState"sv};
    const std::string message{"123456 gGoState"};
    std::string result{message};
    auto found = RRCP::find_response_msg(result, "123456");
    expect(found);
    expect(expected == result);
  };

  "doNotfind_error_response_msg"_test = []
  {
    constexpr std::string_view expected{"E:1"sv};
    const std::string message{"E:1"};
    std::string result{message};
    auto found = RRCP::find_response_msg(result, "0815");
    expect(found);
    expect(expected == result);
  };

  "find_error_response_msg"_test = []
  {
    constexpr std::string_view expected{"E:10"sv};
    const std::string message{"E:10 123456"};
    std::string result{message};
    auto found = RRCP::find_response_msg(result, "123456");
    expect(found);
    expect(expected == result);
  };

  "doNotfind_response_msg"_test = []
  {
    constexpr std::string_view expected{"d NoGo"sv};
    const std::string message{expected};
    std::string result{message};
    auto found = RRCP::find_response_msg(result, "123456");
    expect(!found);
    expect(expected == result);
  };

  // ============================================================

  "insertAfterFirstWord"_test = []
  {
    constexpr std::string_view expected{"M:test 123456 GGoState"sv};
    const std::string command{"M:test GGoState"};
    auto result = RRCP::insertAfterFirstWord(command, "123456");
    expect(expected == result);
  };

  "doNotInsertAnEmpty"_test = []
  {
    constexpr std::string_view expected{"M:test GGoState"sv};
    const std::string command{expected};
    auto result = RRCP::insertAfterFirstWord(command, "");
    expect(expected == result);
  };

  "doNotInsertBeforeTrapCmd"_test = []
  {
    constexpr std::string_view expected{"M:test TGoState1"sv};
    const std::string command{expected};
    auto result = RRCP::insertAfterFirstWord(command, "");
    expect(expected == result);
  };

  "doNotInsertAfterSingleWord"_test = []
  {
    constexpr std::string_view expected{"E:10"sv};
    const std::string message{expected};
    auto result = RRCP::insertAfterFirstWord(message, "123456");
    expect(expected == result);
  };

  // ============================================================

  "create_command_msg"_test = []
  {
    constexpr std::string_view expected{"\nM:test 1 SGoState1\r"sv};
    const std::string command{"M:test SGoState1"};
    std::string msg_id_str;
    int counter{RRCP::INVALID_ID};
    auto result = RRCP::create_command_msg(command, msg_id_str, counter);
    expect(1 == counter);
    expect(expected == result);
  };

  // ============================================================

  "wrong_quoted"_test = []
  {
    expect(throws(
        []
        {
          constexpr std::string_view wrong_quoted{"\n\x1b\004\r"sv};
          auto result = RRCP::esc2char(wrong_quoted);
        }));
  };

  "empty_str"_test = []
  {
    expect(nothrow(
        []
        {
          auto result = RRCP::esc2char("");
          expect(result.empty());
        }));
  };

  "single_esc_msg"_test = [] { expect(throws([] { auto result = RRCP::esc2char("\x1b\rSINGLE_ESC"); })); };

  "esc_as_last_msg"_test = [] { expect(throws([] { auto result = RRCP::esc2char("ESC_AS_LAST\x1b"); })); };

  "to_short_msg"_test = [] { expect(throws([] { auto result = RRCP::esc2char("\x1b\0"s); })); };

  "basic_quoteing"_test = []
  {
    constexpr std::string_view binary{"\nAB_(\0\001\002\003\004\005\006\a\b\n\r\t\v\x1b\20\'\"\?)-CD\r"sv};
    auto quoted = RRCP::char2esc(binary);

    // NOTE: std::quoted works only with std::stringstream
#if defined(BOOST_UT_HAS_FORMAT) && defined(FIXME)
    std::ostringstream binary_bin;
    binary_bin << std::quoted(binary);
    ut::log("{} {}\n", binary.length(), binary_bin.str());

    std::ostringstream quoted_bin;
    quoted_bin << std::quoted(quoted);
    ut::log("{} {}\n", quoted.length(), quoted_bin.str());
#endif

    expect(binary == RRCP::esc2char(quoted));
    expect(binary.length() < quoted.length());
    expect(binary.length() == 28);
    expect(quoted.length() == 33);
  };
};

int main() {}
