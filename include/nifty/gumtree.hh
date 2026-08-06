#ifndef NIFTY_GUMTREE_H
#define NIFTY_GUMTREE_H

#include <llvm/Analysis/RegionInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

#include "nifty/diff.hh"
#include "nifty/perf.hh"
namespace nifty {

struct GumNode {
  /** Entry block for the region, null for an instruction */
  llvm::BasicBlock *block = nullptr;
  /** Region (or null, if this is a leaf)*/
  llvm::Region *region = nullptr;
  /** Instruction (null otherwise) */
  llvm::Instruction *instr = nullptr;
  /** Is this a region, block, or an instruction */
  bool is_region = false, is_block = false, is_instr = false;
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

  // Whether marked as dirty
  bool dirty = false;

  /** Construct a node for the instruction */
  GumNode(llvm::Instruction *instr,
          const llvm::DenseMap<llvm::Value *, uint64_t> &cache);
  /** Construct a node for the basic block */
  GumNode(llvm::BasicBlock *block,
          const llvm::DenseMap<llvm::Value *, uint64_t> &cache);
  /** Construct a node for the region */
  GumNode(llvm::Region *region,
          const llvm::DenseMap<llvm::Value *, uint64_t> &cache);

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

  /** Stable container for GumNode's */
  std::list<GumNode> *gumnodes;

  // LCA search space, mapping the height of the tree to the dirty nodes that
  // will provide the full verification coverage
  // COCKA2 TODO
  llvm::MapVector<unsigned, llvm::DenseSet<GumNode *>> lca_map = {};
  llvm::MapVector<unsigned, unsigned> lca_map_count = {};

  /** Construct a GumTree by diff'ing the two functions */

  GumTree(llvm::Function *src, llvm::Function *dst,
          llvm::RegionInfo *src_regions, llvm::RegionInfo *dst_regions,
          bool refine_top_down = false, double match_threshold = 0.5,
          DiffStats *diff_stats = new DiffStats(), std::list<GumNode> *gumnodes = {});

  void compute_lca_map(llvm::SmallVector<GumNode *> dirty, DiffStats *diff_stats);
};

void get_instruction_gumnodes(llvm::DenseSet<GumNode *> *nodes, GumNode *node);

} // namespace nifty

#endif // NIFTY_GUMTREE_H
