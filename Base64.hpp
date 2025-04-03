#ifndef BASE64_HPP
#define BASE64_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace RRCP::Common
{

class Base64
{
 public:
  /**
   * Constructor for the Base64 class.
   */
  Base64() = default;

  /**
   * Destructor for the Base64 class.
   */
  ~Base64() = default;

  /**
   * Set the line break flag for encoding.
   * @param lbrk If true, the encoded string will have a maximum line length of 80 characters.
   */
  void setLineBreak(bool lbrk) { encodeWithLinebreak_ = lbrk; }

  /**
   * Encode binary data to base64.
   * @param data The data to be encoded.
   * @return The corresponding base64 encoded string.
   */
  [[nodiscard]] static auto encode(std::string_view data) -> std::string;

  /**
   * Decode a Base64 encoded string.
   * @param in The base64 encoded string.
   * @return The decoded string.
   */
  [[nodiscard]] static auto decode(std::string_view in) -> std::string;

 private:
  /**
   * Check if a character is a valid Base64 character.
   * @param c The character to check.
   * @return True if the character is a valid Base64 character, false otherwise.
   */
  [[nodiscard]] auto isBase64Char(char c) const -> bool;

  /**
   * Get the value of a Base64 character.
   * @param c The Base64 character.
   * @return The value of the character (0-63).
   */
  [[nodiscard]] auto base64CharValue(char c) const -> std::uint8_t;

  const std::string_view BaseChars_{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
  bool encodeWithLinebreak_{false};
};

}  // namespace RRCP::Common

#endif  // BASE64_HPP
