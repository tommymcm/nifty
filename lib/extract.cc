#include "nifty/extract.hh"
#include "nifty/assert.hh"
#include "nifty/cast.hh"
#include "nifty/print.hh"

#include <llvm/ADT/SetVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>

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

      // Get the correct value operand iterator.
      auto op_iterator = inst.operands();
      if (auto *phi = dyn_cast<llvm::PHINode>(&inst))
        op_iterator = phi->incoming_values();

      // Check the operands to see if any were defined outside.
      for (llvm::Use &use : op_iterator) {
        llvm::Value *operand = use.get();

        // Skip constants and basic blocks.
        if (isa<llvm::Constant>(operand) or isa<llvm::BasicBlock>(operand))
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

      // Check if the defined value is:
      //  1. used outside the block set
      //  2. used in an exit terminator
      for (llvm::Use &use : inst.uses()) {
        auto *user_inst = dyn_cast<llvm::Instruction>(use.getUser());

        // Ignore non-instruction users.
        if (not user_inst)
          continue;

        // Fetch the user block.
        llvm::BasicBlock *user_block = user_inst->getParent();

        // If the user is non-local, mark value as live-out.
        if (not blockset.contains(user_block))
          live_out.push_back(&inst);

        // Is the user a terminator?
        bool terminator = user_inst == user_block->getTerminator();

        // If the user is NOT a terminator, skip it.
        if (not terminator)
          continue;

        // If the terminator has a target outside the blockset, mark value as
        // live-out.
        // NOTE: This is necessary to handle non-local control dependences.
        bool local_jump = true;
        for (llvm::BasicBlock *succ_block : llvm::successors(user_block)) {
          // Skip local blocks.
          if (blockset.contains(succ_block))
            continue;

          local_jump = false;
          break;
        }

        // If the jump is local, skip the use.
        if (local_jump)
          continue;

        // Otherwise, this is a live-out value.
        live_out.push_back(&inst);
      }
    }
  }

  // Determine the output module.
  llvm::Module *out_module = options.out_module;
  if (not out_module)
    out_module = module;

  // Create globals for the input and output values.
  if (options.verbose) {
    println("==== LIVE IN  ====");
    for (llvm::Value *value : live_in) {
      if (isa<llvm::Argument>(value))
        println("  ", value_name(*value));
      else
        println(*value);
    }
    println();

    println("==== LIVE OUT ====");
    for (llvm::Value *value : live_out) {
      if (isa<llvm::Argument>(value))
        println("  ", value_name(*value));
      else
        println(*value);
    }
    println();
  }

  // Create a new function with the extracted blocks.

  return nullptr;
}

} // namespace nifty
