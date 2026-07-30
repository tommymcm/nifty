#pragma once
// HEADERS
#include <iostream>
#include <ostream>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

struct GumNode;
// NAMESPACES
using namespace std;
namespace nifty {
struct FuncInfo {
  signed gum_node_height;
  unsigned num_instr;
  std::string name = "FuncInfo unitialized";

  unsigned global_var = 0, global_var_write = 0, global_var_read = 0, call = 0,
           load = 0, store = 0, alloca = 0, gep = 0, unary = 0, binary = 0,
           bitwise_logic = 0, uncond_br = 0, cond_br = 0, ret = 0;

  // Default constructor
  FuncInfo() {}
  // Constructor for the whole function
  FuncInfo(llvm::Function *fn) : FuncInfo(fn, -1) {}
  // Constructor for an object that represents an internal GumTree node
  FuncInfo(llvm::Function *fn, signed height) : gum_node_height(height) {
    this->num_instr = fn->getInstructionCount();
    this->name = fn->getName().data();
    for (llvm::BasicBlock &block : *fn) {
      for (llvm::Instruction &instr : block) {
        if (llvm::CallInst::classof(&instr)) this->call++;
        if (llvm::LoadInst::classof(&instr)) {
          this->load++;
          llvm::LoadInst *load_instr = llvm::dyn_cast<llvm::LoadInst>(&instr);
          llvm::Value *ptr = load_instr->getPointerOperand();
          llvm::Value *base_ptr = ptr->stripPointerCasts();
          if (llvm::isa<llvm::GlobalVariable>(base_ptr))
            this->global_var_read++;
        }

        if (llvm::StoreInst::classof(&instr)) {
          this->store++;
          llvm::StoreInst *store_instr =
              llvm::dyn_cast<llvm::StoreInst>(&instr);
          llvm::Value *ptr = store_instr->getPointerOperand();
          llvm::Value *base_ptr = ptr->stripPointerCasts();
          if (llvm::isa<llvm::GlobalVariable>(base_ptr))
            this->global_var_write++;
        }

        if (llvm::AllocaInst::classof(&instr)) this->alloca++;
        if (llvm::GetElementPtrInst::classof(&instr)) this->gep++;
        if (instr.isUnaryOp()) this->unary++;
        if (instr.isBinaryOp()) this->binary++;
        if (instr.isBitwiseLogicOp()) this->bitwise_logic++;
        if (llvm::UncondBrInst::classof(&instr)) this->uncond_br++;
        if (llvm::CondBrInst::classof(&instr)) this->cond_br++;
        if (llvm::ReturnInst::classof(&instr)) this->ret++;

        auto op_iterator = instr.operands();
        for (llvm::Use &use : op_iterator) {
          llvm::Value *op = use.get();
          if (llvm::isa<llvm::GlobalVariable>(op)) this->global_var++;
        }
      }
    }
  }
};

struct DiffStats {
  double build_tree_src_duration = 0.0, build_tree_tgt_duration = 0.0,
         build_tree_duration = 0.0, gum_tree_duration = 0.0,
         top_down_duration = 0.0, bottom_up_duration = 0.0,
         refine_top_down_duration = 0.0, dirty_duration = 0.0,
         erase_duration = 0, find_lca_duration = 0;

  bool lca_computed = false;
  bool lca_dirty = false;
  // COCKA2 TODO: change to unsigned, and change the constructor
  unsigned src_tree_height = 0, tgt_tree_height = 0;
  signed lca_height = -1;
  unsigned lca_subtree_size = 0, lca_dirty_subtree_size = 0;

  // extract_node_duration[i] = extract_node_src_duration[i] +
  // extract_node_tgt_duration[i]
  llvm::SmallVector<double> extract_node_duration = {},
                            extract_node_src_duration = {},
                            extract_node_tgt_duration = {};

  // LCA search space, mapping the height of the tree to the dirty nodes that
  // will provide the full verification coverage
  llvm::MapVector<unsigned, unsigned> lca_map_count = {};

  // The heights of each of the internal GumTree nodes on the path from LCA to
  // the root
  llvm::SmallVector<unsigned> pairs_heights = {};
};

// Performance stats for the verification procedure of a single LLVM function
struct PerfStats {
  enum STATUS {
    ERROR,
    TYPE_CHECKER_FAILED,
    SYNTACTIC_EQ,
    CORRECT,
    UNSOUND,
    FAILED_TO_PROVE,
    NOT_VERIFIED
  };
  std::ostream &out;
  // Full src and tgt functions
  FuncInfo src_info, tgt_info;

  // Time stats for each verification performed while traversing the GumTree
  llvm::SmallVector<pair<pair<FuncInfo, FuncInfo>, double>> gum_verify = {};
  llvm::SmallVector<STATUS> gum_results = {};
  double gum_verify_duration_total = 0.0, max_gum_verify_duration = 0.0,
         min_gum_verify_duration = numeric_limits<double>::max();
  unsigned max_gum_verify_id = -1, min_gum_verify_id = -1;
  // LCA stats
  unsigned path_length = 0;

  // Whether the source and target functions are isomorphic
  bool isomorphic = false;

  // Whether the full function was validated
  bool full_func = false;
  double full_func_duration = -1.0;
  STATUS full_func_result = STATUS::NOT_VERIFIED;

  // Verification results
  unsigned gum_failed = 0, gum_unsound = 0, gum_correct = 0, gum_error = 0,
           gum_type_checker = 0;

  // EXEC STATS

  DiffStats *diff_stats;

  double verify_duration_total = 0.0;

  void gum_tree_stats();
  void time_stats();
  void fn_stats(const FuncInfo *fn_info);
  void verify_stats(const FuncInfo *src_info, const FuncInfo *tgt_info,
                    signed index);
  // TO PRINT STATUS
  std::string status_to_string(STATUS status);

  PerfStats(std::ostream &out, llvm::Function *src_fn, llvm::Function *tgt_fn)
      : out(out), src_info(src_fn), tgt_info(tgt_fn) {
    FuncInfo add_src(src_fn), add_tgt(tgt_fn);
    this->src_info = add_src;
    this->tgt_info = add_tgt;
  }
};

} // namespace nifty