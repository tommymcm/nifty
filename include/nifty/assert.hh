#ifndef NIFTY_ASSERT_H
#define NIFTY_ASSERT_H

#include <cassert>
#include <cstdio>
#include <string>
#include <type_traits>

#include "nifty/print.hh"

#define _NIFTY_ASSERT(pretty_func, pretty_line, c, msg...)                     \
  if (!(c)) {                                                                  \
    nifty::println("\n");                                                      \
    nifty::println("\x1b[31m====================================");            \
    nifty::println("\x1b[1;31mNifty Assert Failed!\x1b[0m");                   \
    nifty::println("  \x1b[1;33m", msg, "\x1b[0m");                            \
    nifty::println("  in \x1b[1;35m", pretty_func, "\x1b[0m");                 \
    nifty::println("  at \x1b[1;35m", pretty_line, "\x1b[0m");                 \
    nifty::println("\x1b[31m====================================\x1b[0m");     \
    nifty::println();                                                          \
    assert(c);                                                                 \
  }

#define NIFTY_ASSERT(c, msg...)                                                \
  _NIFTY_ASSERT(__PRETTY_FUNCTION__, __LINE__, c, msg)

#define NIFTY_UNREACHABLE(msg...) NIFTY_ASSERT(false, msg)

#endif // NIFTY_ASSERT_H
