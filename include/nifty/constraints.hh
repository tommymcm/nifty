#ifndef NIFTY_CONSTRAINTS_H
#define NIFTY_CONSTRAINTS_H

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace nifty {

struct ConstraintOptions {
  // None yet :)
};

/**
 * Infer constraints in the given function from conditional checks and insert as
 * explicit llvm.assume calls in the branch targets.
 *
 * @returns true if the function was modified, false otherwise.
 */
bool infer_constraints(llvm::Function *function,
                       ConstraintOptions options = {});

/**
 * Infer constraints in the given module from conditional checks and insert as
 * explicit llvm.assume calls in the branch targets.
 *
 * @returns true if the function was modified, false otherwise.
 */
bool infer_constraints(llvm::Module &module, ConstraintOptions options = {});

} // namespace nifty

#endif
