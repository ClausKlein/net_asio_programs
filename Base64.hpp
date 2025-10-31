#ifndef BASE64_HPP
#define BASE64_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace rrcp::common
{

class base64
{
 public:
  /**
   * Constructor for the Base64 class.
   */
  base64() = default;

  /**
   * Destructor for the Base64 class.
   */
  ~base64() = default;

  /**
   * Set the line break flag for encoding.
   * @param lbrk If true, the encoded string will have a maximum line length of 80 characters.
   */
  void set_line_break(bool lbrk) { encode_with_linebreak_ = lbrk; }

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
  [[nodiscard]] auto is_base64_char(char c) const -> bool;

  /**
   * Get the value of a Base64 character.
   * @param c The Base64 character.
   * @return The value of the character (0-63).
   */
  [[nodiscard]] auto base64_char_value(char c) const -> std::uint8_t;

  const std::string_view base_chars_{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
  bool encode_with_linebreak_{false};
};

}  // namespace rrcp::common

#endif  // BASE64_HPP
