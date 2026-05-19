#ifndef NIFTY_SESE_H
#define NIFTY_SESE_H

#include <llvm/Analysis/RegionInfo.h>

namespace nifty {

/**
 * Compute the minimal SESE difference between the two functions.
 */
void sese_diff(llvm::Function &before, llvm::Function &after);

} // namespace nifty

#endif // NIFTY_SESE_H
