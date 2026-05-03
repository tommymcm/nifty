#ifndef NIFTY_STRIP_H
#define NIFTY_STRIP_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

namespace nifty {

/**
 * Strip metadata of the given kind(s) from the instruction.
 * @returns true if the instruction was modified, false otherwise
 */
bool strip(llvm::Instruction &instruction, llvm::ArrayRef<unsigned> kinds);

/**
 * Strip metadata of the given kind(s) from the function.
 * @returns true if the function was modified, false otherwise
 */
bool strip(llvm::Function &function, llvm::ArrayRef<unsigned> kinds);

/**
 * Strip metadata of the given kind(s) from the module.
 * @returns true if the module was modified, false otherwise
 */
bool strip(llvm::Module &module, llvm::ArrayRef<unsigned> kinds);

} // namespace nifty

#endif // NIFTY_STRIP_H
