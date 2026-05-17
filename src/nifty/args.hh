#include <llvm/Support/CommandLine.h>

// ====---- Subcommands ----==== //
static llvm::cl::

    static llvm::cl::opt<std::string>
        opt_inpath(cl::Positional, cl::desc("Input file"), cl::Required);
static llvm::cl::opt<std::string> opt_outpath("o",
                                              cl::desc("Output file"),
                                              cl::value_desc("filename"));
