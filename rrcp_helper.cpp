#include "rrcp_helper.hpp"

#include <fmt/format.h>

#include <boost/algorithm/string/predicate.hpp>  // for starts_with
#include <boost/algorithm/string/trim.hpp>  // for trim_left, trim_right
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

constexpr char ESC = 0x1B;
constexpr char REPLACE_LF = 0x01;
constexpr char REPLACE_CR = 0x02;
constexpr char REPLACE_ESC = 0x03;

auto rrcp::esc2char(std::string_view data) -> std::string
{
  std::string message;
  auto len = data.size();
  for (size_t i = 0; i < len; ++i)
  {
    char ch = data[i];

    if (ch == STOP)
    {
      return message;
    }

    if (ch == ESC)
    {
      if (i == len - 1)
      {
        throw std::runtime_error("esc2char: Error - message ends with escape character!");
      }

      char const next = data[++i];
      switch (next)
      {
      case REPLACE_LF:
        ch = '\n';
        break;
      case REPLACE_CR:
        ch = '\r';
        break;
      case REPLACE_ESC:
        ch = ESC;
        break;
      default:
        throw std::runtime_error("esc2char: Error - unexpected ESC character!");
      }
    }

    message.push_back(ch);
  }
  return message;
}

auto rrcp::char2esc(std::string_view data) -> std::string
{
  std::string message;
  for (char const ch : data)
  {
    switch (ch)
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
      message.push_back(ch);
      break;
    }
  }
  return message;
}

auto rrcp::insertAfterFirstWord(const std::string& input, const std::string& toInsert) -> std::string
{
  if (toInsert.empty())
  {
    return input;  // Nothing to do
  }

  size_t firstSpace = input.find_first_of(" \t");  // Find first whitespace
  if (firstSpace == std::string::npos)
  {
    return input;  // No spaces found, return original string
  }

  // NOTE: Only if Set/Get command request, NOT for Trap commands!
  size_t nextNonSpace = input.find_first_of("SG", firstSpace);
  if (nextNonSpace == std::string::npos)
  {
    // If there's no second valid command, just return the input!
    // XXX return input + " " + toInsert;
    return input;
  }

  // If there's valid command, just append toInsert after the first word
  return input.substr(0, firstSpace + 1) + toInsert + " " + input.substr(nextNonSpace);
}

auto rrcp::find_response_msg(std::string& response, const std::string& msg_id) -> bool
{
  // DEBUG: fmt::print("RRCP MU received({})\n", response);
  // NOTE: different order for error responses like this: "E:2 10001"
  auto pos = response.find(msg_id);
  if (pos != std::string::npos)
  {
    // Remove the inserted message number for Set/Get responses.
    if (boost::algorithm::starts_with(response, msg_id))
    {
      // NOTE: This is an Command response with msg_id!
      response = response.substr(pos + msg_id.length());
      boost::trim_left(response);
    }
    else
    {
      // NOTE: This is an Error response with msg_id!
      response = response.substr(0, pos);
      boost::trim_right(response);
    }
    return true;
  }

  // NOTE: This is an Error response with or w/o a valid msg_id!
  if (boost::algorithm::starts_with(response, "E:"))
  {
    return true;  // return, this may be a response to an Trap command?
  }

  return false;
}

auto rrcp::create_command_msg(const std::string& message, std::string& msg_id_str, int msg_id) -> std::string
{
  if(msg_id >= INVALID_ID)
  {
    msg_id = 1;
  }
  // Insert the next message number for Set/Get request.
  // But prevent to insert the msg_id for Trap commands!
  auto trap_cmd = message.find(" T");
  if (trap_cmd == std::string::npos)
  {
    msg_id_str = fmt::format("{}", (msg_id));
  }
  else
  {
    msg_id_str.clear();
  }
  std::string msg = insertAfterFirstWord(message, msg_id_str);

  // DEBUG: fmt::print("rrcp MU to send({})\n", msg);

  // Create the RRCP message frame
  std::string command = char2esc(msg);
  command.insert(0, 1, START);
  command += STOP;

  return command;
}
