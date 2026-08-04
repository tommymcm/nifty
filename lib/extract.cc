#include "nifty/extract.hh"
#include "nifty/assert.hh"
#include "nifty/cast.hh"
#include "nifty/print.hh"
#include "nifty/regions.hh"

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <iostream>
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
        if (not blockset.contains(user_block)) {
          live_out.push_back(&inst);
          break;
        }

        // Is the user a terminator?
        bool terminator = user_inst == user_block->getTerminator();
      
        // Is the user a branching condition?
        bool cond_br = (dyn_cast<llvm::CondBrInst>(user_inst))? true : false;

        // If the user is NOT a terminator, skip it.
        if (not terminator || cond_br) continue;

        // If the terminator has a target outside the blockset, mark value as
        // live-out.
        // NOTE: This is necessary to handle non-local control dependencies.
        bool local_jump = true;
        for (llvm::BasicBlock *succ_block : llvm::successors(user_block)) {
          // Check if the jump is local.
          local_jump &= blockset.contains(succ_block);
        }

        // If the jump is local, skip the use.
        if (local_jump)
          continue;

        // Otherwise, this is a live-out value.
        live_out.push_back(&inst);
        break;
      }
    }
  }

  // Collect the set of exiting edges from the blockset.
  llvm::SmallVector<llvm::BasicBlockEdge> exit_edges;
  for (llvm::BasicBlock *block : blocks) {
    for (llvm::BasicBlock *succ : llvm::successors(block)) {
      // Skip local edges.
      if (blockset.contains(succ))
        continue;
      // Register the non-local edge.
      exit_edges.emplace_back(block, succ);
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
      // Fetch the type, and skip invalid types.
      auto *type = value->getType();
      if (not type)
        continue;

      // Skip metadata.
      if (type->isMetadataTy())
        continue;

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

      // If we've already created a global for this value, skip it.
      if (globals.contains(value))
        continue;

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
      auto [_it, _fresh] = globals.try_emplace(value, global);
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
  for (llvm::BasicBlock *pred : llvm::predecessors(first_block)) {
    // Skip local blocks.
    if (blockset.contains(pred))
      continue;

    vmap[pred] = entry_block;
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

    debugln("  VAR ", *global);

    debugln("  VAR ", *global);

    // Declare a function that emits the given type
    llvm::FunctionType *func =
        llvm::FunctionType::get(orig_value->getType(), {}, false);

    // Getting the string of the type
    std::string ty;
    llvm::raw_string_ostream ty_stream(ty);
    orig_value->getType()->print(ty_stream);
    ty = "load_" + ty;
    llvm::FunctionCallee func_callee =
        out_module->getOrInsertFunction(ty, func);
    llvm::CallInst *load_call =
        builder.CreateCall(func_callee, {}, orig_value->getName());

    debugln("  VAL ", *load_call);

    // Map the original value to the load.
    vmap[orig_value] = load_call;
  }

  // Jump into the first block in the array, assumed to be the single-entry.
  builder.CreateBr(first_block);


  // Create the exit block which stores pseudoglobal values as well as the
  // exiting branch. COCKA2 TODO modify exit_edges to only have an edge towards
  // the exit block.
  auto *cocka2_exit_block =
      llvm::BasicBlock::Create(context, "cocka2_exit",out_function);
  
  builder.SetInsertPoint(cocka2_exit_block);
  llvm::Type *ret_type = out_function->getReturnType();
  if (ret_type->isVoidTy()) builder.CreateRetVoid();
  else
    builder.CreateRet(llvm::Constant::getNullValue(ret_type));

  // Nonlocal blocks which are exit targets.
  llvm::DenseMap<llvm::BasicBlockEdge, llvm::BasicBlock *> exit_blocks;

  
  // debugln("==== ADD EXIT BLOCK BRANCHES ====");
  for(llvm::BasicBlockEdge &edge : exit_edges){
    auto [_it, fresh] = exit_blocks.try_emplace(edge, cocka2_exit_block);
    NIFTY_ASSERT(fresh, "Cloned the same exit edge twice, something is off");
  }
  
  

  // Clone over all of the other blocks.
  debugln("==== CLONE BLOCKS ====");
  for (llvm::BasicBlock *orig_block : blocks) {
    // Clone the basic block.
    llvm::BasicBlock *clone_block =
        llvm::CloneBasicBlock(orig_block, vmap,
                              /* suffix */ "",
                              /* function */ out_function);

    vmap[orig_block] = clone_block;

  }

  llvm::PHINode *exit_phi = NULL;
  // Non-local exits get a reverse post-order value

  if (exit_edges.size()) {
    llvm::DenseSet<llvm::BasicBlock *> seen_start_blocks;
    builder.SetInsertPoint(cocka2_exit_block->getTerminator());
    llvm::ReversePostOrderTraversal<llvm::Function *> rpo(function);
    exit_phi = builder.CreatePHI(llvm::Type::getInt64Ty(context), exit_edges.size());

    for (const llvm::BasicBlockEdge &edge : exit_edges) {
      llvm::BasicBlock *start_block =
          const_cast<llvm::BasicBlock *>(edge.getStart());
      llvm::BasicBlock *end_block =
          const_cast<llvm::BasicBlock *>(edge.getEnd());
      
      // Even if there are two non-local exits, the "cocka2_exit_value" must be defined only once
      if(seen_start_blocks.contains(start_block)) continue;
      seen_start_blocks.insert(start_block);

      llvm::BasicBlock *cloned_start = dyn_cast<llvm::BasicBlock>(vmap[start_block]);
      llvm::Value *cocka2_exit_value;

      llvm::Instruction *cloned_term = cloned_start->getTerminator();
      builder.SetInsertPoint(cloned_term);

      llvm::CondBrInst *cond_br = dyn_cast<llvm::CondBrInst>(cloned_term);
      llvm::UncondBrInst *uncond_br = dyn_cast<llvm::UncondBrInst>(cloned_term);

      NIFTY_ASSERT(
          cond_br || uncond_br,
          "Non-local exit targets must arise from branch instructions!");
      // Unconditional branch -> just the block's reverse post order
      if (uncond_br) {
        // Get the non-local target block

        llvm::BasicBlock *non_local = uncond_br->getSuccessor();

        llvm::Constant *zero =
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
        llvm::Constant *order = NULL;

        unsigned i = 0;
        for (llvm::BasicBlock *block : rpo) {
          if (block == non_local) {
            order = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i);
            break;
          }

          i++;
        }

        NIFTY_ASSERT(order, "Non-local exit block " +
                                non_local->getNameOrAsOperand() +
                                " not found in the function");

        cocka2_exit_value = builder.CreateAdd(
            zero, order,
            "cocka2_exit_value_" + cloned_start->getNameOrAsOperand());

      }
      // Conditional branch -> exit_value based on select of the condition
      else {
        llvm::BasicBlock *non_local1 = cond_br->getSuccessor(0);
        llvm::BasicBlock *non_local2 = cond_br->getSuccessor(1);
        llvm::Value *br_cond = cond_br->getCondition();
        llvm::Constant *order1 = NULL, *order2 = NULL;

        unsigned i = 0;
        for (llvm::BasicBlock *block : rpo) {
          if (block == non_local1)
            order1 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i);
          if (block == non_local2)
            order2 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i);

          if (order1 && order2) break;
          i++;
        }

        NIFTY_ASSERT(order1 && order2, "At least one of the non-local exit "
                                       "blocks out of : " +
                                           non_local1->getNameOrAsOperand() +
                                           " and " +
                                           non_local2->getNameOrAsOperand() +
                                           " not found in the function!");
        cocka2_exit_value = builder.CreateSelect(
            br_cond, order1, order2,
            "cocka2_exit_value_" + cloned_start->getNameOrAsOperand());
      }
      // Add the branch value in the phi node.
      exit_phi->addIncoming(cocka2_exit_value, cloned_start);
      // phi_blocks.try_emplace(cocka2_exit_value, start_block);
    }

    // Consume the value of non-local exit block.
    builder.SetInsertPoint(cocka2_exit_block->getTerminator());
      llvm::FunctionType *func = llvm::FunctionType::get(
          llvm::Type::getVoidTy(context), llvm::Type::getInt64Ty(context), false);
      // Getting the string of the type
      std::string ty = "store_exit";
      llvm::FunctionCallee func_callee =
          out_module->getOrInsertFunction(ty, func);
      llvm::CallInst *store_call =
          builder.CreateCall(func_callee, {exit_phi});
    
  }

for(llvm::BasicBlock *orig_block : blocks){
  llvm::BasicBlock *clone_block = dyn_cast<llvm::BasicBlock>(vmap[orig_block]);
  // Patch up any of the exiting edges.
    llvm::Instruction *terminator = clone_block->getTerminator();
    for (llvm::BasicBlock *succ_block : llvm::successors(orig_block)) {
      llvm::BasicBlockEdge edge(orig_block, succ_block);
      llvm::BasicBlock *exit_block = exit_blocks.lookup(edge);
      if (not exit_block) continue;

      terminator->replaceSuccessorWith(succ_block, exit_block);
    }
  
  }

  
  // Map old arguments to new arguments.
  for (const auto &[old_arg, new_arg] :
       llvm::zip(function->args(), out_function->args()))
    vmap[&old_arg] = &new_arg;

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
      // builder.CreateStore(clone_value, global);

      // Declare a function that uses the given type
      llvm::FunctionType *func =
          llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                  { orig_value->getType() },
                                  false);
      // Getting the string of the type
      std::string ty;
      llvm::raw_string_ostream ty_stream(ty);
      orig_value->getType()->print(ty_stream);
      ty = "store_" + ty;
      llvm::FunctionCallee func_callee =
          out_module->getOrInsertFunction(ty, func);
      llvm::CallInst *store_call =
          builder.CreateCall(func_callee, { clone_value });
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
