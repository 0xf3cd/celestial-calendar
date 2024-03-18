#ifndef __UTIL_FORMATTER_HPP__
#define __UTIL_FORMATTER_HPP__

#include <string>

namespace util {

/*! 
 * @fn format
 * @brief Helper function to format string.
 * @param format The format string.
 * @param args The arguments to be formatted.
 * @return The formatted string.
 * @todo Use std::format when it is supported by the compiler.
 */
template <typename... Args> 
std::string format(const std::string& format, Args... args) {
  // Reference： https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
  // Author: https://stackoverflow.com/users/2533467/ifreilicht
  const int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
  if (size_s <= 0) { 
    throw std::runtime_error { "Error during formatting." }; 
  }
  const auto size = static_cast<size_t>(size_s);

  std::unique_ptr<char[]> buf { new char[size] };
  std::snprintf(buf.get(), size, format.c_str(), args...);

  const auto final_log = std::string_view { buf.get(), buf.get() + size - 1 }; // We don't want the '\0' inside
  return std::string { final_log };
}

} // namespace util

#endif // __UTIL_FORMATTER_HPP__
