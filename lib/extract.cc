#include "nifty/extract.hh"
#include "nifty/assert.hh"
#include "nifty/cast.hh"

#include <llvm/ADT/SetVector.h>

namespace nifty {

llvm::BasicBlock *parent(llvm::Value *value) {
  auto *inst = dyn_cast<llvm::Instruction>(value);
  if (not inst)
    return nullptr;
  return inst->getParent();
}

llvm::Function *extract(llvm::ArrayRef<llvm::BasicBlock *> blocks,
                        ExtractOptions options) {
  // If no blocks were provided, return NULL.
  if (blocks.empty())
    return nullptr;

  // Fetch the parent context.
  llvm::Function *function = blocks.front()->getParent();
  NIFTY_ASSERT(function, "Block does not have parent function!");
  llvm::Module *module = function->getParent();
  NIFTY_ASSERT(module, "Function does not have parent module!");

  // Construct the blockset.
  llvm::SetVector<llvm::BasicBlock *> blockset(blocks.begin(), blocks.end());

  // Collect the set of "live-in" and "live-out" values.
  // live-in values are used in the blocks, but defined outside.
  // live-out valies are defined in the blocks, but used outside.
  llvm::SmallVector<llvm::Value *, 0> live_in, live_out;
  llvm::DenseSet<llvm::Value *> seen;

  for (llvm::BasicBlock *block : blocks) {
    for (llvm::Instruction &inst : *block) {
      // Check the operands to see if any were defined outside.
      for (llvm::Value *operand : inst.operand_values()) {
        // Skip constants.
        if (llvm::isa<llvm::Constant>(operand))
          continue;

        // Skip operands we've already seen.
        if (seen.contains(operand))
          continue;
        seen.insert(operand);

        // If the value is defined in the set of blocks, skip it.
        llvm::BasicBlock *op_block = parent(operand);
        if (blockset.contains(op_block))
          continue;

        // Otherwise, this is a live-in value.
        live_in.push_back(operand);
      }

      // Check if the defined value is used outside the block set.
      for (llvm::Use &use : inst.uses()) {
        auto *user_inst = dyn_cast<llvm::Instruction>(use.getUser());

        // Ignore non-instruction users.
        if (not user_inst)
          continue;

        // If the user is in the set of blocks, skip it.
        llvm::BasicBlock *user_block = user_inst->getParent();
        if (blockset.contains(user_block))
          continue;

        // Otherwise, this is a live-out value.
        live_out.push_back(&inst);
      }
    }
  }

  // Create globals for the input and output values.
  // llvm::GlobalVariable in(module);
  // llvm::GlobalVariable out(module);

  // Create a new function.

  return nullptr;
}

} // namespace nifty
