#ifndef NIFTY_CONSTRAINTS_H
#define NIFTY_CONSTRAINTS_H

namespace nifty {

struct ConstraintOptions {
  /** Propagate constraints to uses. */
  bool propagate = false;
};

/**
 * Infer constraints from conditional checks and insert as explicit llvm.assume
 * calls in the branch targets.
 *
 * @returns true if the function was modified, false otherwise.
 */
bool infer_constraints(llvm::Function *function,
                       ConstraintOptions options = {});

} // namespace nifty

#endif
