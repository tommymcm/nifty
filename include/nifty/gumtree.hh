#ifndef NIFTY_GUMTREE_H
#define NIFTY_GUMTREE_H

#include <llvm/Analysis/RegionInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

namespace nifty {

struct GumNode {
  /** Entry block for the region */
  llvm::BasicBlock *block;
  /** Region (or null, if this is a leaf)*/
  llvm::Region *region = nullptr;
  /** Block hash */
  uint64_t label;
  /** Combined block hashes of children */
  uint64_t subtree_hash;
  /** Height in tree */
  unsigned height = 0;
  /** Postorder traversal index, for ordering during matching*/
  int postorder_index = 0;

  /** Direct parent region, NULL if root */
  GumNode *parent = nullptr;
  /** Direct children of this region node */
  llvm::SmallVector<GumNode *> children;

  // Matching state
  GumNode *match = nullptr;

  /** Construct a node for the basic block */
  GumNode(llvm::BasicBlock *block);
  /** Construct a node for the region */
  GumNode(llvm::Region *region);

  /** Fetch a postorder traversal of the node */
  llvm::SmallVector<GumNode *> postorder();

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &, GumNode *);
};

/** Mapping between matched GumTree nodes */
using GumMatches = typename llvm::DenseMap<GumNode *, GumNode *>;

struct GumTree {
  /** Tree roots */
  GumNode *src, *dst;
  /** Matched nodes */
  GumMatches matches;

  /** Construct a GumTree by diff'ing the two functions */
  GumTree(llvm::Function *src,
          llvm::Function *dst,
          llvm::RegionInfo *src_regions,
          llvm::RegionInfo *dst_regions,
          bool refine_top_down = false,
          double match_threshold = 0.5);
};

} // namespace nifty

#endif // NIFTY_GUMTREE_H
