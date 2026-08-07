#ifndef NIFTY_EXTRACT_H
#define NIFTY_EXTRACT_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SetVector.h>
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
  /** Modules for extracted functions, used in coextraction */
  llvm::Module *src_out_module = nullptr, *tgt_out_module = nullptr;
};
/**
 * @brief Computes and stores live-in and live-out values given selected basic
 * blocks
 */
void collect_live(llvm::ArrayRef<llvm::BasicBlock *> blocks,
                  llvm::SetVector<llvm::BasicBlock *> *blockset,
                  llvm::SmallVector<llvm::Value *, 0> *live_in,
                  llvm::SmallVector<llvm::Value *, 0> *live_out);

// REGION COEXTRACTION
std::pair<llvm::Function *, llvm::Function *> extract(
    llvm::Region *src,
    llvm::Region *tgt,
    llvm::DenseMap<llvm::Value *, llvm::Value *> *vmatchings,
    ExtractOptions options);

// COEXTRACTION
std::pair<llvm::Function *, llvm::Function *> extract(
    llvm::ArrayRef<llvm::BasicBlock *> src_blocks,
    llvm::ArrayRef<llvm::BasicBlock *> tgt_blocks,
    llvm::DenseMap<llvm::Value *, llvm::Value *> *vmatchings,
    ExtractOptions options);

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
