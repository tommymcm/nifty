#include <set>

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>

#include "nifty/cast.hh"
#include "nifty/constraints.hh"

namespace nifty {

struct Constraint {
  llvm::Value *condition = nullptr;
  bool inverted = false;

  Constraint(llvm::Value *condition, bool inverted = false)
    : condition{ condition },
      inverted{ inverted } {}
};

bool infer_constraints(llvm::Function *function, ConstraintOptions options) {
  bool modified = false;

  // Ensure we have a valid function.
  if (not function)
    return false;

  // Identify blocks that can have constraints inserted.
  llvm::DenseMap<llvm::BasicBlock *, std::set<std::pair<llvm::Value *, bool>>>
      to_insert;
  for (llvm::BasicBlock &block : *function) {
    llvm::Instruction *terminator = block.getTerminator();

    // Handle conditional branches.
    auto *branch = dyn_cast<llvm::CondBrInst>(terminator);
    if (not branch)
      continue;

    // Fetch the condition and see if it is a comparison.
    llvm::Value *condition = branch->getCondition();
    if (not condition)
      continue; // shouldn't ever happen, but we are being safe ;)
    auto *comparison = dyn_cast<llvm::CmpInst>(condition);

    // Are we sole predecessor of the branch targets?
    llvm::BasicBlock *then_block = branch->getSuccessor(0),
                     *else_block = branch->getSuccessor(1);

    if (then_block->getSinglePredecessor() == &block)
      to_insert[then_block].insert({ condition, false });

    if (else_block->getSinglePredecessor() == &block)
      to_insert[else_block].insert({ condition, true });
  }

  // TODO: Propagate to all relevant uses.

  // Insert assumptions.
  llvm::IRBuilder<> builder(function->getContext());
  for (const auto &[block, constraints] : to_insert) {
    // Insert at the beginning of the block.
    builder.SetInsertPoint(block->getFirstInsertionPt());

    // Insert llvm.assume instructions for each call.
    for (auto [condition, inverted] : constraints) {
      // If the condition is an instruction, clone it here.
      if (auto *condition_inst = dyn_cast<llvm::Instruction>(condition)) {
        llvm::Instruction *condition_clone = condition_inst->clone();
        builder.Insert(condition_clone);

        // Use the clone as the condition for our assumption.
        condition = condition_clone;
      }

      // Insert the assumption.
      builder.CreateAssumption(condition);
      modified |= true;
    }

    // Reset the builder.
    builder.ClearInsertionPoint();
  }

  return modified;
}

} // namespace nifty
