#include <boost/ut.hpp>  // import boost.ut;
#include <iomanip>  // use std::quoted
#include <sstream>
#include <string>
#include <string_view>

#define DEBUG

#include "rrcp_helper.hpp"
#include "rrcp_message.hpp"

namespace ut = boost::ut;

ut::suite errors = [] -> void
{
  using namespace ut;
  using namespace std::literals;

  "find_response_msg"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"gGoState"sv};
    const std::string message{"123456 gGoState"};
    std::string result{message};
    auto found = rrcp::find_response_msg(result, "123456");
    expect(found);
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  "doNotfind_error_response_msg"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"E:1"sv};
    const std::string message{"E:1"};
    std::string result{message};
    auto found = rrcp::find_response_msg(result, "0815");
    expect(found);
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  "find_error_response_msg"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"E:10"sv};
    const std::string message{"E:10 123456"};
    std::string result{message};
    auto found = rrcp::find_response_msg(result, "123456");
    expect(found);
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  "doNotfind_response_msg"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"d NoGo"sv};
    const std::string message{EXPECTED};
    std::string result{message};
    auto found = rrcp::find_response_msg(result, "123456");
    expect(!found);
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  // ============================================================

  "insertAfterFirstWord"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"M:test 123456 GGoState"sv};
    const std::string command{"M:test GGoState"};
    auto result = rrcp::insertAfterFirstWord(command, "123456");
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  "doNotInsertAnEmptyString"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"M:test GGoState"sv};
    const std::string command{EXPECTED};
    auto result = rrcp::insertAfterFirstWord(command, "");
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  "doNotInsertBeforeTrapCmd"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"M:test TGoState1"sv};
    const std::string command{EXPECTED};
    auto result = rrcp::insertAfterFirstWord(command, "");
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  "doNotInsertAfterSingleWord"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"E:10"sv};
    const std::string message{EXPECTED};
    auto result = rrcp::insertAfterFirstWord(message, "123456");
    expect(EXPECTED == result);
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", result, EXPECTED);
#endif
  };

  // ============================================================

  "create_command_msg"_test = [] -> void
  {
    constexpr std::string_view EXPECTED{"\nM:RxTx 1 SPowerLevel\"Off\"\r"sv};
    const std::string command{R"(M:RxTx SPowerLevel"Off")"};
    std::string msg_id_str;
    int counter{rrcp::INVALID_ID};
    auto result = rrcp::create_command_msg(command, msg_id_str, counter);
    expect("1" == msg_id_str);
    expect(EXPECTED == result);

    std::ostringstream quoted;
    quoted << std::quoted(result.substr(1, result.length() - 1));  // NOTE: w/o START STOP
#ifdef BOOST_UT_HAS_FORMAT
    ut::log("{} == {}\n", "RRCP MU", quoted.str());
#endif
  };

  // ============================================================

  "wrong_quoted"_test = [] -> void
  {
    expect(throws(
        [] -> void
        {
          constexpr std::string_view WRONG_QUOTED{"\n\x1b\004\r"sv};
          auto result = rrcp::esc2char(WRONG_QUOTED);
        }));
  };

  "empty_str"_test = [] -> void
  {
    expect(nothrow(
        [] -> void
        {
          auto result = rrcp::esc2char("");
          expect(result.empty());
        }));
  };

  "single_esc_msg"_test = [] -> void { expect(throws([] -> void { auto result = rrcp::esc2char("\x1b\rSINGLE_ESC"); })); };

  "esc_as_last_msg"_test = [] -> void { expect(throws([] -> void { auto result = rrcp::esc2char("ESC_AS_LAST\x1b"); })); };

  "to_short_msg"_test = [] -> void { expect(throws([] -> void { auto result = rrcp::esc2char("\x1b\0"s); })); };

  "basic_quoteing"_test = [] -> void
  {
    constexpr std::string_view BINARY{"\nAB_(\0\001\002\003\004\005\006\a\b\n\r\t\v\x1b\20\'\"\?)-CD\r"sv};
    auto quoted = rrcp::char2esc(BINARY);

    // NOTE: std::quoted works only with std::stringstream
#if defined(BOOST_UT_HAS_FORMAT)
    std::ostringstream binary_bin;
    binary_bin << std::quoted(BINARY);
    ut::log("{} {}\n", BINARY.length(), binary_bin.str());

    std::ostringstream quoted_bin;
    quoted_bin << std::quoted(quoted);
    ut::log("{} {}\n", quoted.length(), quoted_bin.str());
#endif

    expect(BINARY == rrcp::esc2char(quoted));
    expect(BINARY.length() < quoted.length());
    expect(BINARY.length() == 28);
    expect(quoted.length() == 33);
  };

  // ============================================================

  "rrcp_message"_test = [] -> void
  {
    rrcp_message msg;
    msg.body_length(MAX_MU_LENGTH);
    expect(msg.length() == MAX_MU_LENGTH + 4);
    msg.encode_body();
    expect(msg.body_length() == MAX_MU_LENGTH);
    // XXX expect(msg.is_valid());
    expect(msg.decode_body());

    msg.encode_header();
    expect(msg.length() == MAX_MU_LENGTH + 4);
    expect(msg.decode_header());

    msg.clear();
    expect(!msg.is_valid());
    expect(msg.get_body().empty());
    expect(msg.get_data().length() == 4);
  };

  "rrcp_message_empty"_test = [] -> void
  {
    rrcp_message msg;
    msg.body_length(0);
    expect(msg.length() == 4);
    msg.encode_body();
    expect(msg.body_length() == 0);
    expect(!msg.is_valid());
    expect(!msg.decode_body());

    // FIXME: expect(nothrow([&] {msg.decode_header();} ));
  };

  "rrcp_message_to_long"_test = [] -> void
  {
    const std::string invalid(MAX_MU_LENGTH, '\n');
    rrcp_message msg(invalid);
    expect(!msg.is_valid());
    expect(!msg.set_msg(invalid));
  };

  "rrcp_message_text"_test = [] -> void
  {
    constexpr std::string_view COMMAND{"Hallo Server"};
    rrcp_message msg;
    expect(msg.set_msg(COMMAND));
    expect(msg.is_valid());
    expect(msg.body_length() == COMMAND.length());
    expect(msg.body() == COMMAND);
    auto result = msg.get_msg();
    expect(COMMAND == result);
  };

  "rrcp_message_binary"_test = [] -> void
  {
    constexpr std::string_view BINARY{"\nAB_(\0\001\002\003\004\005\006\a\b\n\r\t\v\x1b\20\'\"\?)-CD\r"sv};
    rrcp_message msg(BINARY);
    expect(msg.is_valid());
    expect(msg.body_length() == 33);
    auto result = msg.get_msg();
    expect(BINARY == result);
  };

  // ============================================================
};

auto main() -> int {}
