#ifndef NIFTY_EXTRACT_H
#define NIFTY_EXTRACT_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

namespace nifty {

struct ExtractOptions {
  /** Preserve the value of constant globals? */
  bool keep_const_init = false;
  /** Module to construct the extracted function(s). */
  llvm::Module *out_module = nullptr;
  /** Enable verbose debug printing. */
  bool verbose = false;
};

/**
 * Extract basic blocks into their own function.
 * We assume that the blocks have a single-entry.
 * We assume the first block in the array is the single-entry.
 */
llvm::Function *extract(llvm::ArrayRef<llvm::BasicBlock *> blocks,
                        ExtractOptions options);

} // namespace nifty

#endif // NIFTY_EXTRACT_H
