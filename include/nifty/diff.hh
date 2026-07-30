#pragma once

#ifndef NIFTY_DIFF_H
#define NIFTY_DIFF_H

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/MapVector.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

#include <nifty/perf.hh>

namespace nifty {

struct DiffOptions {
  /** Module to construct the diff'd function(s). */
  llvm::Module *out_module = nullptr;
  /** Diff all diff'd region(s). */
  bool regions = false;
  /** Refine missed matches by re-running top-down after bottom-up matches */
  bool refine_top_down = true;
  /** Threshold for region matching. 0.0 (always match) - 1.0 (only exact) */
  double match_threshold = 0.1;
  /** Dump GumTree */
  bool dump_gumtree = true;
  /** Optional isomorphic matching, applied in case roots do not match or if LCA
   * is the whole function */
  bool isomophic_matching = false;

  // TOREMOVE
  /** Threshold for LCA */
  double lca_threshold = 0.5;
  // TOREMOVE
};


struct DiffResult {
  llvm::MapVector<llvm::Function *, llvm::Function *> pairs;
  DiffStats *diff_stats;

  DiffResult() : pairs({}) { diff_stats = new DiffStats(); }
  DiffResult(DiffStats *diff_stats) : pairs({}), diff_stats(diff_stats) {}
};

/**
 * Helper type to hold extracted diff(s).
 * Contains an ordered list of diff'd function pairs.
 */

/**
 * Diff two functions, extracting their differences into their own functions.
 */
DiffResult diff(llvm::Function *src, llvm::Function *tgt,
                DiffOptions options = {});

} // namespace nifty

#endif // NIFTY_DIFF_H
