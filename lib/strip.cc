#include "nifty/strip.hh"

#include <llvm/IR/InstIterator.h>

namespace nifty {

// ====---- Helpers ----====
static bool has_kind(unsigned search, llvm::ArrayRef<unsigned> kinds) {
  for (unsigned kind : kinds)
    if (kind == search)
      return true;

  return false;
}

// ====---- Strip Instruction ----====
bool strip(llvm::Instruction &instruction, llvm::ArrayRef<unsigned> kinds) {
  bool modified = false;

  instruction.eraseMetadataIf([&](unsigned kind, llvm::MDNode *node) {
    bool found = has_kind(kind, kinds);
    modified |= found;
    return found;
  });

  return modified;
}

// ====---- Strip Function ----====
bool strip(llvm::Function &function, llvm::ArrayRef<unsigned> kinds) {
  bool modified = false;

  // Strip metadata from the function.
  function.eraseMetadataIf([&](unsigned kind, llvm::MDNode *node) {
    bool found = has_kind(kind, kinds);
    modified |= found;
    return found;
  });

  // Strip selected metadata from each instruction in the function.
  for (llvm::Instruction &instruction : llvm::instructions(function))
    modified |= strip(instruction, kinds);

  return modified;
}

// ====---- Strip Module ----====
bool strip(llvm::Module &module, llvm::ArrayRef<unsigned> kinds) {
  bool modified = false;

  // Strip metadata from the globals.
  for (llvm::GlobalVariable &global : module.globals()) {
    global.eraseMetadataIf([&](unsigned kind, llvm::MDNode *node) {
      bool found = has_kind(kind, kinds);
      modified |= found;
      return found;
    });
  }

  // Recurse on each function.
  for (llvm::Function &function : module.functions()) {
    modified |= strip(function, kinds);
  }

  return modified;
}

} // namespace nifty
