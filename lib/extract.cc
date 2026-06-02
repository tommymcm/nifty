#include "nifty/extract.hh"
#include "nifty/assert.hh"
#include "nifty/cast.hh"
#include "nifty/print.hh"
#include "nifty/regions.hh"

#include <llvm/ADT/SetVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/Cloning.h>

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
  llvm::BasicBlock *first_block = blocks.front();
  llvm::Function *function = first_block->getParent();
  NIFTY_ASSERT(function, "Block does not have parent function!");
  llvm::Module *module = function->getParent();
  NIFTY_ASSERT(module, "Function does not have parent module!");
  llvm::LLVMContext &context = module->getContext();

  // Construct the blockset.
  llvm::SetVector<llvm::BasicBlock *> blockset(blocks.begin(), blocks.end());

  // Collect the set of "live-in" and "live-out" values.
  // live-in values are used in the blocks, but defined outside.
  // live-out valies are defined in the blocks, but used outside.
  llvm::SmallVector<llvm::Value *, 0> live_in, live_out;
  llvm::DenseSet<llvm::Value *> seen;
  llvm::SmallVector<llvm::BasicBlockEdge> exit_edges;

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

          // Register the non-local exit edge.
          exit_edges.emplace_back(user_block, succ_block);

          // Mark the jump as non-local.
          local_jump = false;
        }

        // If the jump is local, skip the use.
        if (local_jump)
          continue;

        // Otherwise, this is a live-out value.
        live_out.push_back(&inst);
      }
    }
  }

  // Dump analysis information.
  {
    debugln("==== LIVE IN  ====");
    for (llvm::Value *value : live_in) {
      if (isa<llvm::Argument>(value))
        debugln("  ", value_name(*value));
      else
        debugln(*value);
    }
    debugln();

    debugln("==== LIVE OUT ====");
    for (llvm::Value *value : live_out) {
      if (isa<llvm::Argument>(value))
        debugln("  ", value_name(*value));
      else
        debugln(*value);
    }
    debugln();
  }

  // Determine the output module.
  llvm::Module *out_module = options.out_module;
  if (not out_module)
    out_module = module;

  // Ensure that the LLVMContext of the input and output modules match.
  NIFTY_ASSERT(&context == &out_module->getContext(),
               "mismatched LLVMContext between input/output modules");

  // Create globals for the input and output values.
  llvm::DenseMap<llvm::Value *, llvm::GlobalVariable *> globals;
  for (auto &values : { live_in, live_out }) {
    for (llvm::Value *value : values) {
      // Skip arguments.
      if (isa<llvm::Argument>(value))
        continue;

      // Clone global values.
      if (isa<llvm::GlobalValue>(value)) {
        // If we are emitting to the same module, there's no need to clone.
        if (out_module == module)
          continue;

        // TODO: Implement global value cloning.
        NIFTY_UNREACHABLE("NYI: global value cloning ");
      }

      // Create the global variable.
      auto *global = new llvm::GlobalVariable(
          *out_module,
          value->getType(),
          /* constant? */ false,
          llvm::GlobalVariable::LinkageTypes::ExternalLinkage,
          /* initializer */ nullptr);

      {
        debugln("CREATE GLOBAL");
        debugln("  ", *global);
        debugln("  FOR ", value_name(*value));
      }

      // Map the original value to the new global.
      auto [_it, fresh] = globals.try_emplace(value, global);
      NIFTY_ASSERT(fresh, "Failed to record global!");
    }
  }

  // Create a new function with the extracted blocks.
  llvm::Function *out_function = llvm::Function::Create(
      function->getFunctionType(),
      llvm::GlobalVariable::LinkageTypes::ExternalLinkage,
      function->getName(), // NOTE: consumers should rename.
      out_module);

  // Create a value mapper.
  llvm::ValueToValueMapTy vmap;

  // Create the entry block.
  auto *entry_block = llvm::BasicBlock::Create(context, "entry", out_function);

  // Map all non-local incoming block to the entry block.
  for (llvm::BasicBlock *succ : llvm::predecessors(first_block)) {
    // Skip local blocks.
    if (blockset.contains(succ))
      continue;

    vmap[succ] = entry_block;
  }

  // Create a builder.
  llvm::IRBuilder<> builder(entry_block);

  // Populate the entry block with global variable loads.
  for (llvm::Value *orig_value : live_in) {
    {
      debugln("LOAD GLOBAL");
      debugln("  FOR ", *orig_value);
    }

    // Load the value from its global.
    llvm::GlobalVariable *global = globals.lookup(orig_value);
    if (not global)
      continue;

    { debugln("  VAR ", *global); }

    auto *load_value = builder.CreateLoad(orig_value->getType(),
                                          global,
                                          orig_value->getName());

    { debugln("  VAL ", *load_value); }

    // Map the original value to the load.
    vmap[orig_value] = load_value;
  }

  // Jump into the first block in the array, assumed to be the single-entry.
  builder.CreateBr(first_block);

  // For each non-local branch target, create an exit block.
  debugln("==== CREATE EXIT BLOCKS ====");
  llvm::DenseMap<llvm::BasicBlockEdge, llvm::BasicBlock *> exit_blocks;
  for (const llvm::BasicBlockEdge &edge : exit_edges) {
    const llvm::BasicBlock *start_block = edge.getStart();
    const llvm::BasicBlock *end_block = edge.getEnd();

    auto *exit_block = llvm::BasicBlock::Create(
        context,
        "exit." + start_block->getName() + "." + end_block->getName(),
        out_function);

    auto [_it, fresh] = exit_blocks.try_emplace(edge, exit_block);
    NIFTY_ASSERT(fresh, "Cloned the same exit edge twice, something is off");

    // Insert a default return value.
    // NOTE: Alternatively, we could call exit() with an unreachable.
    llvm::Type *ret_type = out_function->getReturnType();
    llvm::Value *ret_value = llvm::Constant::getNullValue(ret_type);
    builder.SetInsertPoint(exit_block);
    builder.CreateRet(ret_value);
  }

  // Clone over all of the other blocks.
  debugln("==== CLONE BLOCKS ====");
  for (llvm::BasicBlock *orig_block : blocks) {
    // Clone the basic block.
    llvm::BasicBlock *clone_block =
        llvm::CloneBasicBlock(orig_block,
                              vmap,
                              /* suffix */ "",
                              /* function */ out_function);

    vmap[orig_block] = clone_block;

    // Patch up any of the exiting edges.
    llvm::Instruction *terminator = clone_block->getTerminator();
    for (llvm::BasicBlock *succ_block : llvm::successors(orig_block)) {
      llvm::BasicBlockEdge edge(orig_block, succ_block);
      llvm::BasicBlock *exit_block = exit_blocks.lookup(edge);
      if (not exit_block)
        continue;

      terminator->replaceSuccessorWith(succ_block, exit_block);
    }
  }

  // Map old arguments to new arguments.
  for (const auto &[old_arg, new_arg] :
       llvm::zip(function->args(), out_function->args())) {
    vmap[&old_arg] = &new_arg;
  }

  for (auto &block : *out_function) {
    for (auto &inst : block) {
      for (auto &operand : inst.operands()) {
        llvm::Value *value = operand.get();
        if (!isa<llvm::Constant>(value) && !vmap.count(value)) {
          println("Missing from VMap: ", *value);
        }
      }
    }
  }

  // Remap values.
  debugln("==== REMAP VALUES ====");
  llvm::ValueMapper mapper(vmap, llvm::RF_IgnoreMissingLocals);
  mapper.remapFunction(*out_function);

  // Store live-out values to global variable before function exit.
  debugln("==== STORE LIVE-OUTS ====");
  llvm::DominatorTree dom_tree(*out_function);
  for (llvm::BasicBlock &block : *out_function) {
    // Fetch the terminator.
    llvm::Instruction *terminator = block.getTerminator();
    NIFTY_ASSERT(terminator, "Block has no terminator ", block);

    // Skip terminators that don't exit the function.
    bool is_exit = isa<llvm::ReturnInst>(terminator)
                   or isa<llvm::ResumeInst>(terminator)
                   or isa<llvm::UnreachableInst>(terminator);
    if (not is_exit)
      continue;

    // Store all live-outs that dominate this location.
    builder.SetInsertPoint(terminator);
    for (llvm::Value *orig_value : live_out) {
      // Fetch the cloned value.
      llvm::Value *clone_value = vmap.lookup(orig_value);
      auto *clone_inst = dyn_cast_or_null<llvm::Instruction>(clone_value);
      NIFTY_ASSERT(clone_inst, "live-out was not cloned ", *orig_value);

      // Skip live-out values that don't dominate this exit.
      if (not dom_tree.dominates(clone_inst, terminator))
        continue;

      // Store the cloned value to its global.
      llvm::GlobalVariable *global = globals.lookup(orig_value);
      NIFTY_ASSERT(global, "Could not find global for ", *orig_value);
      builder.CreateStore(clone_value, global);
    }
  }

  // Return the output function.
  return out_function;
}

llvm::Function *extract(llvm::Region *region, ExtractOptions options) {

  // Construct the basic block array for extraction.
  llvm::SmallVector<llvm::BasicBlock *, 0> blocks;

  // The first element in the array MUST be the entry block.
  llvm::BasicBlock *entry_block = region->getEntry();
  NIFTY_ASSERT(entry_block, "Could not find single-entry block");
  blocks.push_back(entry_block);

  // Append all other blocks.
  for (llvm::BasicBlock *block : region->blocks()) {
    // Skip the entry block, since it's already been added.
    if (block == entry_block)
      continue;

    blocks.push_back(block);
  }

  // Call into the extract helper.
  llvm::Function *out_function = extract(blocks, options);

  // Return the extracted function.
  return out_function;
}

static void walk_region_tree(const ExtractOptions &options,
                             llvm::Region *region,
                             unsigned depth = 0) {
  // Extract this region.
  extract(region, options);

  // Recurse on all subregions.
  for (std::unique_ptr<llvm::Region> &subregion : *region)
    walk_region_tree(options, subregion.get(), depth + 1);

  return;
}

void extract(llvm::RegionInfo *region_tree, ExtractOptions options) {
  // Extract all regions from the region tree.
  walk_region_tree(options, region_tree->getTopLevelRegion());

  return;
}

void extract(llvm::Function *function, ExtractOptions options) {

  // Extract the selected region(s).
  if (options.regions) {
    // Construct the region info (program structure tree).
    llvm::RegionInfo *region_info = regions(function);

    // Extract region tree.
    extract(region_info, options);

    return;
  }

  // By default, just extract the entire function.
  llvm::SmallVector<llvm::BasicBlock *, 16> blocks;
  for (llvm::BasicBlock &block : *function)
    blocks.push_back(&block);

  extract(blocks, options);

  return;
}

} // namespace nifty
