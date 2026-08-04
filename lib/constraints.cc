#include <set>

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>

#include "nifty/assert.hh"
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

void insert_uses(std::set<llvm::Use *> &uses, llvm::Value *value) {
  for (llvm::Use &use : value->uses())
    uses.insert(&use);
}

bool infer_constraints(llvm::Function *function, ConstraintOptions options) {
  bool modified = false;

  // Ensure we have a valid function.
  NIFTY_ASSERT(function, "Given NULL function");

  // Skip empty functions.
  if (function->empty())
    return false;

  // Construct the dominator tree.
  llvm::DominatorTree domtree(*function);

  // Identify blocks that can have constraints inserted.
  llvm::DenseMap<llvm::BasicBlock *, std::set<std::pair<llvm::Value *, bool>>>
      to_assume;
  for (llvm::BasicBlock &block : *function) {
    llvm::Instruction *terminator = block.getTerminator();

    // Handle conditional branches.
    auto *branch = dyn_cast<llvm::CondBrInst>(terminator);
    if (not branch)
      continue;
    llvm::BasicBlock *then_block = branch->getSuccessor(0),
                     *else_block = branch->getSuccessor(1);

    // Fetch the condition and see if it is a comparison.
    llvm::Value *condition = branch->getCondition();
    NIFTY_ASSERT(condition, "Found NULL condition");

    // Collect all uses of the condition.
    std::set<llvm::Use *> relevant_uses;
    insert_uses(relevant_uses, condition);

    // If this is a comparison, fetch uses of the compared operands.
    if (auto *comparison = dyn_cast<llvm::CmpInst>(condition)) {
      insert_uses(relevant_uses, comparison->getOperand(0));
      insert_uses(relevant_uses, comparison->getOperand(1));
    }

    // For each use, if its parent block is dominated by the true/false edge, we
    // will insert an assumption.
    for (llvm::Use *use : relevant_uses) {
      NIFTY_ASSERT(use, "Found NULL use");

      // Only handle non-PHI instruction users.
      auto *user = dyn_cast<llvm::Instruction>(use->getUser());
      if (not user or isa<llvm::PHINode>(user))
        continue;

      // Fetch the parent block.
      llvm::BasicBlock *user_block = user->getParent();

      // Is the parent block dominated by the true edge?
      llvm::BasicBlockEdge then_edge(&block, then_block);
      if (domtree.dominates(then_edge, *use))
        to_assume[user_block].emplace(condition, false);

      // Is the parent block dominated by the false edge?
      llvm::BasicBlockEdge else_edge(&block, else_block);
      if (domtree.dominates(else_edge, *use))
        to_assume[user_block].emplace(condition, true);
    }
  }

  // Insert assumptions.
  llvm::IRBuilder<> builder(function->getContext());
  for (const auto &[block, constraints] : to_assume) {
    // Insert at the beginning of the block.
    builder.SetInsertPoint(block->getFirstInsertionPt());

    // Insert llvm.assume instructions for each call.
    for (auto [condition, inverted] : constraints) {
      // If the condition is safe to execute again (i.e., it has no side
      // effects and has the same behavior in a possibly different memory
      // context, then clone it here.
      auto *cond_inst = dyn_cast<llvm::Instruction>(condition);
      if (cond_inst //
          and not cond_inst->mayHaveSideEffects()
          and not cond_inst->mayReadFromMemory()) {

        llvm::Instruction *condition_clone = cond_inst->clone();
        builder.Insert(condition_clone);

        // Use the clone as the condition for our assumption.
        condition = condition_clone;
      }

      if (inverted) {
        // Invert the condition.
        llvm::Value *false_value =
            llvm::Constant::getNullValue(condition->getType());

        // Invert by checking if the value is false.
        condition = builder.CreateICmpEQ(condition, false_value);
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

bool infer_constraints(llvm::Module &module, ConstraintOptions options) {
  bool modified = false;

  for (llvm::Function &function : module) {
    modified |= infer_constraints(&function, options);
  }

  return modified;
}

} // namespace nifty
