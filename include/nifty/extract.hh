#ifndef NIFTY_EXTRACT_H
#define NIFTY_EXTRACT_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Analysis/RegionInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

namespace nifty {

struct ExtractOptions {
  /** Preserve the value of constant globals? */
  bool keep_const_init = false;
  /** Module to construct the extracted function(s). */
  llvm::Module *out_module = nullptr;
  /** Extract all SESE regions */
  bool regions = false;
};

/**
 * Extract basic blocks into their own function.
 * We assume that the blocks have a single-entry.
 * We assume the first block in the array is the single-entry.
 */
llvm::Function *extract(llvm::ArrayRef<llvm::BasicBlock *> blocks,
                        ExtractOptions options);

/**
 * Extract single-entry-single-exit region into its own function.
 */
llvm::Function *extract(llvm::Region *region, ExtractOptions options);

/**
 * Extract all single-entry-single-exit regions into their own functions.
 */
void extract(llvm::RegionInfo *region_tree, ExtractOptions options);

/**
 * Extract all single-entry-single-exit regions from function into their own
 * functions.
 */
void extract(llvm::Function *function, ExtractOptions options);

} // namespace nifty

#endif // NIFTY_EXTRACT_H
