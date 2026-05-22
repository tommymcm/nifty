#ifndef NIFTY_DIFF_H
#define NIFTY_DIFF_H

#include <llvm/ADT/MapVector.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace nifty {

struct DiffOptions {
  /** Module to construct the diff'd function(s). */
  llvm::Module *out_module = nullptr;
  /** Diff all diff'd region(s). */
  bool regions = false;
  /** Refine missed matches by re-running top-down after bottom-up matches */
  bool refine_top_down = false;
  /** Threshold for region matching. 0.0 (always match) - 1.0 (only exact) */
  double match_threshold = 0.5;
  /** Dump GumTree */
  bool dump_gumtree = false;
};

/**
 * Helper type to hold extracted diff(s).
 * Contains an ordered list of diff'd function pairs.
 */
struct DiffResult : llvm::MapVector<llvm::Function *, llvm::Function *> {};

/**
 * Diff two functions, extracting their differences into their own functions.
 */
DiffResult diff(llvm::Function *src,
                llvm::Function *tgt,
                DiffOptions options = {});

} // namespace nifty

#endif // NIFTY_DIFF_H
