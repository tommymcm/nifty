#ifndef NIFTY_PRINT_H
#define NIFTY_PRINT_H

#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

namespace nifty {

extern bool glob_verbosity;

using Colors = llvm::raw_ostream::Colors;
enum class Style { NORMAL = 0, BOLD, RESET };

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const Style &style) {
  switch (style) {
    case Style::NORMAL:
      os.changeColor(Colors::SAVEDCOLOR, false);
      break;
    case Style::BOLD:
      os.changeColor(Colors::SAVEDCOLOR, true);
      break;
    case Style::RESET:
      os.resetColor();
      break;
  }

  return os;
}

// ====---- Printing ----==== //
inline void fprint(llvm::raw_ostream &out) {}

template <class T, class... Ts>
inline void fprint(llvm::raw_ostream &out, T const &first, Ts const &...rest) {
  out << first;
  fprint(out, rest...);
}

template <class... Ts>
inline void fprintln(llvm::raw_ostream &out, Ts const &...args) {
  fprint(out, args..., '\n');
}

template <class... Ts>
inline void println(Ts const &...args) {
  fprintln(llvm::errs(), args...);
}

template <class... Ts>
inline void print(Ts const &...args) {
  fprint(llvm::errs(), args...);
}

// ====---- Warnings ----==== //
template <class... Ts>
inline void fwarn(llvm::raw_ostream &out, Ts const &...args) {
  if (out.has_colors()) {
    out.changeColor(Colors::YELLOW, /*bold=*/true);
  }

  fprint(out, "WARNING: ");

  if (out.has_colors()) {
    out.resetColor();
  }

  fprint(out, args...);
}

template <class... Ts>
inline void fwarnln(llvm::raw_ostream &out, Ts const &...args) {
  fwarn(out, args..., '\n');
}

template <class... Ts>
inline void warnln(Ts const &...args) {
  fwarnln(llvm::errs(), args...);
}

template <class... Ts>
inline void warn(Ts const &...args) {
  fwarn(llvm::errs(), args...);
}

// ====---- Debugging ----==== //
inline void fdebug(llvm::raw_ostream &out) {}

template <class T, class... Ts>
inline void fdebug(llvm::raw_ostream &out, T const &first, Ts const &...rest) {
  if (glob_verbosity) {
    out << first;
    fdebug(out, rest...);
  }
}

template <class... Ts>
inline void fdebugln(llvm::raw_ostream &out, Ts const &...args) {
  fdebug(out, args..., '\n');
}

template <class... Ts>
inline void debugln(Ts const &...args) {
  fdebugln(llvm::errs(), args...);
}

template <class... Ts>
inline void debug(Ts const &...args) {
  fdebug(llvm::errs(), args...);
}

// ====---- Helpers ----==== //
inline std::string value_name(const llvm::Value &V) {
  std::string str;
  llvm::raw_string_ostream os(str);
  V.printAsOperand(os, /* print type = */ false);
  // os << ":" << uint64_t(&V);

  return str;
}

} // namespace nifty

#endif // NIFTY_PRINT_H
