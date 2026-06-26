#pragma once
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#define IS_TTY() (_isatty(_fileno(stdout)) && _isatty(_fileno(stderr)))
#else
#include <unistd.h>
#define IS_TTY() (isatty(fileno(stdout)) && isatty(fileno(stderr)))
#endif

namespace ansi {
constexpr const char *reset = "\033[0m";
constexpr const char *bold = "\033[1m";
constexpr const char *grey = "\033[90m";
constexpr const char *red = "\033[31m";
constexpr const char *yellow = "\033[33m";
constexpr const char *green = "\033[32m";
constexpr const char *cyan = "\033[36m";
constexpr const char *magenta = "\033[35m";
} // namespace ansi

namespace logging {

namespace detail {
// enables ANSI on Windows 10+ consoles, no-op elsewhere
inline void enableWindowsAnsi() {
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(h, &mode))
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

  HANDLE he = GetStdHandle(STD_ERROR_HANDLE);
  DWORD emode = 0;
  if (GetConsoleMode(he, &emode))
    SetConsoleMode(he, emode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

inline bool &colorEnabled() {
  static bool enabled = []() {
    enableWindowsAnsi();
    return IS_TTY();
  }();
  return enabled;
}

inline const char *c(const char *code) { return colorEnabled() ? code : ""; }
} // namespace detail

inline void step(const std::string &msg) {
  std::cout << detail::c(ansi::bold) << detail::c(ansi::green) << "=>"
            << detail::c(ansi::reset) << " " << detail::c(ansi::bold) << msg
            << detail::c(ansi::reset) << "\n";
}

inline void info(const std::string &msg) {
  std::cout << detail::c(ansi::grey) << "  " << msg << detail::c(ansi::reset)
            << "\n";
}

inline void source(const std::string &src, const std::string &msg) {
  std::cout << "  " << detail::c(ansi::cyan) << "[" << src << "]"
            << detail::c(ansi::reset) << " " << msg << "\n";
}

inline void warn(const std::string &msg) {
  std::cerr << "  " << detail::c(ansi::yellow) << "[warn]"
            << detail::c(ansi::reset) << " " << msg << "\n";
}

inline void error(const std::string &msg) {
  std::cerr << detail::c(ansi::bold) << detail::c(ansi::red) << "  [error]"
            << detail::c(ansi::reset) << " " << msg << "\n";
}

} // namespace logging