#include <cstdlib>

#include "nifty/print.hh"

namespace nifty {

static int get_default_verbosity() {
  if (const char *env = std::getenv("NIFTY_VERBOSE"))
    return std::atoi(env);
#ifdef NDEBUG
  return 0;
#else
  return 1;
#endif
}

// Provide a default value for verbose output.
// Tools that links with nifty can override this.
__attribute__((weak)) bool glob_verbosity = get_default_verbosity();

} // namespace nifty
