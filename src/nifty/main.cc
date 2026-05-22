#include <string>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

#include "nifty/assert.hh"
#include "nifty/diff.hh"
#include "nifty/extract.hh"
#include "nifty/print.hh"
#include "nifty/strip.hh"

using namespace nifty;
using namespace llvm;

// ====---- Subcommands ----==== //
cl::SubCommand cmd_extract("extract", "Extract parts of the IR");
cl::SubCommand cmd_diff("diff", "SESE difference of two IRs");
cl::SubCommand cmd_strip_tbaa("strip-tbaa", "Strip TBAA metadata");

// ====---- Options ----==== //
cl::opt<std::string> opt_inpath( //
    cl::Positional,
    cl::desc("<input>"),
    cl::sub(cmd_extract),
    cl::sub(cmd_diff),
    cl::sub(cmd_strip_tbaa));

cl::opt<std::string> opt_outpath( //
    "o",
    cl::desc("output file"),
    cl::sub(cmd_extract),
    cl::sub(cmd_diff),
    cl::sub(cmd_strip_tbaa));

cl::opt<bool> opt_verbose( //
    "v",
    cl::desc("verbose output"),
    cl::sub(cl::SubCommand::getAll()));

bool nifty::glob_verbosity = opt_verbose;

// -------- Extract options -------- //
cl::opt<std::string> opt_extract_function( //
    "func",
    cl::desc("function to extract from"),
    cl::sub(cmd_extract));

cl::opt<bool> opt_extract_regions( //
    "regions",
    cl::desc("extract all SESE regions"),
    cl::sub(cmd_extract));

// -------- Diff options -------- //
cl::opt<std::string> opt_diff_path( //
    cl::Positional,
    cl::desc("<other input>"),
    cl::sub(cmd_diff));

cl::opt<std::string> opt_diff_function( //
    "func",
    cl::desc("function to diff"),
    cl::sub(cmd_diff));

cl::opt<bool> opt_diff_refine_top_down( //
    "refine-top-down",
    cl::desc("refine matching with extra top-down passes"),
    cl::sub(cmd_diff));

cl::opt<double> opt_diff_match_threshold( //
    "match-threshold",
    cl::desc("Minimum similarity score (0.0-1.0) required to match two "
             "structurally changed regions. Lower values match more "
             "aggressively across large transformations. (default: 0.5)"),
    cl::value_desc("value"),
    cl::init(0.5),
    cl::sub(cmd_diff));

cl::opt<bool> opt_diff_dump_gumtree( //
    "dump-gumtree",
    cl::desc("dump intermediate GumTree to stdout"),
    cl::sub(cmd_diff));

// ====---- Dispatch ----==== //
int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "Nifty LLVM utilities\n");

  StringRef inpath = opt_inpath;

  LLVMContext context;
  SMDiagnostic error;
  std::unique_ptr<Module> module = parseIRFile(inpath, error, context);
  if (!module) {
    error.print(argv[0], errs());
    return 1;
  }

  if (cmd_extract) {

    // Find the function.
    Function *function = nullptr;
    if (opt_extract_function.getNumOccurrences() > 0) {
      function = module->getFunction(opt_extract_function);
    } else {
      function = &*module->begin();
    }

    // Inform the user if we couldn't find their favorite function...
    if (not function) {
      println("ERROR: failed to find function (", opt_extract_function, ")");
      return 1;
    }

    // Convert flags to library options.
    ExtractOptions options = { .regions = opt_extract_regions };

    // Extract!
    extract(function, options);

  } else if (cmd_diff) {
    // Load the other module.
    StringRef diffpath = opt_diff_path;
    std::unique_ptr<Module> other_module =
        parseIRFile(diffpath, error, context);
    if (!module) {
      error.print(argv[0], errs());
      return 1;
    }

    // Find the function.
    Function *function = nullptr;
    if (opt_diff_function.getNumOccurrences() > 0) {
      function = module->getFunction(opt_extract_function);
    } else {
      function = &*module->begin();
    }

    // Inform the user if we couldn't find their favorite function...
    if (not function) {
      println("ERROR: failed to find function (", opt_extract_function, ")");
      return 1;
    }

    // Find the function in the other module.
    StringRef function_name = function->getName();
    Function *other_function = other_module->getFunction(function_name);
    if (not other_function) {
      println("ERROR: failed to find other function (", function_name, ")");
      return 1;
    }

    // Compute and extract the diff.
    DiffOptions options = { .refine_top_down = opt_diff_refine_top_down,
                            .match_threshold = opt_diff_match_threshold,
                            .dump_gumtree = opt_diff_dump_gumtree };
    diff(function, other_function, options);

  } else if (cmd_strip_tbaa) {
    strip(*module.get(), { LLVMContext::MD_tbaa });
  }

  // Handle output file, if needed.
  StringRef outpath = opt_outpath;
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
