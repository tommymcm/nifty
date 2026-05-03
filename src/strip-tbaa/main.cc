#include <string>

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

#include "nifty/strip.hh"

using namespace nifty;
using namespace llvm;

static cl::opt<std::string> opt_inpath(cl::Positional,
                                       cl::desc("Input file"),
                                       cl::Required);
static cl::opt<std::string> opt_outpath("o",
                                        cl::desc("Output file"),
                                        cl::value_desc("filename"));

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "Strip TBAA metadata\n");

  StringRef inpath = opt_inpath;
  StringRef outpath = opt_outpath;

  LLVMContext context;
  SMDiagnostic error;
  auto module = parseIRFile(inpath, error, context);
  if (!module) {
    error.print(argv[0], errs());
    return 1;
  }

  strip(*module.get(), { LLVMContext::MD_tbaa });

  if (outpath.empty()) {
    module->print(outs(), nullptr);
  } else {

    bool is_bitcode = outpath.ends_with(".bc");
    auto flag = is_bitcode ? sys::fs::OF_None : sys::fs::OF_Text;

    std::error_code code;
    ToolOutputFile outfile(outpath, code, flag);
    if (code) {
      errs() << code.message() << "\n";
      return 1;
    }

    if (is_bitcode) {
      WriteBitcodeToFile(*module, outfile.os());
    } else {
      module->print(outfile.os(), nullptr);
    }

    outfile.keep();
  }

  return 0;
}
