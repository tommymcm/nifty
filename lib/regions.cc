#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/IR/Dominators.h>

#include "nifty/regions.hh"

namespace nifty {

llvm::RegionInfo *regions(llvm::Function *function) {
  // Construct the region info (program structure tree).
  auto *dom_tree = new llvm::DominatorTree(*function);

  auto *post_dom_tree = new llvm::PostDominatorTree(*function);

  auto *dom_frontier = new llvm::DominanceFrontier();
  dom_frontier->analyze(*dom_tree);

  auto *region_info = new llvm::RegionInfo();
  region_info->recalculate(*function, dom_tree, post_dom_tree, dom_frontier);

  return region_info;
}

} // namespace nifty
