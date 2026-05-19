#ifndef NIFTY_REGIONS_H
#define NIFTY_REGIONS_H

#include <llvm/Analysis/RegionInfo.h>
#include <llvm/IR/Function.h>

namespace nifty {

/**
 * Compute the LLVM RegionInfo (program structure tree) for the given function.
 */
llvm::RegionInfo *regions(llvm::Function *function);

} // namespace nifty

#endif // NIFTY_REGIONS_H
