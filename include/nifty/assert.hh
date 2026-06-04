#ifndef NIFTY_ASSERT_H
#define NIFTY_ASSERT_H

#include <cassert>
#include <cstdio>
#include <string>
#include <type_traits>

#include "nifty/print.hh"

#define _NIFTY_ASSERT(func, file, line, c, msg...)                             \
  if (!(c)) {                                                                  \
    ::nifty::println();                                                        \
    ::nifty::println(::nifty::Colors::RED,                                     \
                     ::nifty::Style::BOLD,                                     \
                     "Nifty Assertion Failed:",                                \
                     ::nifty::Colors::RESET);                                  \
    ::nifty::println(::nifty::Colors::YELLOW,                                  \
                     ::nifty::Style::BOLD,                                     \
                     msg,                                                      \
                     ::nifty::Colors::RESET);                                  \
    ::nifty::println(::nifty::Colors::CYAN,                                    \
                     ::nifty::Style::BOLD,                                     \
                     file,                                                     \
                     ::nifty::Colors::RESET,                                   \
                     ":",                                                      \
                     ::nifty::Colors::CYAN,                                    \
                     ::nifty::Style::BOLD,                                     \
                     line,                                                     \
                     ::nifty::Colors::RESET);                                  \
    ::nifty::println(nifty::Colors::MAGENTA,                                   \
                     ::nifty::Style::BOLD,                                     \
                     func,                                                     \
                     ::nifty::Colors::RESET);                                  \
    ::nifty::println();                                                        \
    assert(c);                                                                 \
  }

#define NIFTY_ASSERT(c, msg...)                                                \
  _NIFTY_ASSERT(__PRETTY_FUNCTION__, __FILE__, __LINE__, c, msg)

#define NIFTY_UNREACHABLE(msg...) NIFTY_ASSERT(false, msg)

#endif // NIFTY_ASSERT_H
