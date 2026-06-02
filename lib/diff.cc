#include <llvm/Analysis/RegionInfo.h>

#include "nifty/assert.hh"
#include "nifty/diff.hh"
#include "nifty/extract.hh"
#include "nifty/gumtree.hh"
#include "nifty/regions.hh"

namespace nifty {

static void collect_dirty_src(llvm::SmallVector<GumNode *> &dirty,
                              GumNode *node) {

  if (not node->match) {
    // unmatched => added or removed
    dirty.push_back(node);

  } else if (node->label != node->match->label) {
    // matched, but different hash => modified
    dirty.push_back(node);
  }

  // Recurse on children.
  for (GumNode *child : node->children)
    collect_dirty_src(dirty, child);
}

static void collect_dirty_dst(llvm::SmallVector<GumNode *> &dirty,
                              GumNode *node) {
  // Found an unmatched node.
  if (not node->match) {

    // Walk up to find the lowest matched ancestor in dst, then use its src
    // match as the dirty anchor.
    GumNode *current = node->parent;
    while (current and not current->match)
      current = current->parent;

    if (current) {
      dirty.push_back(node->parent->match);
    }
  }

  // Recurse on children.
  for (GumNode *child : node->children)
    collect_dirty_dst(dirty, child);
}

static llvm::SmallVector<GumNode *> collect_dirty(GumNode *src, GumNode *dst) {
  llvm::SmallVector<GumNode *> dirty;

  collect_dirty_src(dirty, src);
  collect_dirty_dst(dirty, dst);

  return dirty;
}

static GumNode *find_lca(GumNode *src_root,
                         const llvm::SmallVector<GumNode *> &dirty) {
  // Walk each dirty node up to the root.
  llvm::DenseMap<GumNode *, unsigned> ancestor_count;
  for (GumNode *node : dirty) {
    GumNode *current = node;
    while (current) {
      ++ancestor_count[current];
      current = current->parent;
    }
  }

  // The lowest common ancestor is the lowest node whose count equals
  // dirty.size(), i.e., all dirty paths pass through it.
  GumNode *lca = src_root;
  for (GumNode *n : src_root->postorder()) {
    if (ancestor_count[n] == dirty.size()) {
      lca = n;
      break; // postorder gives us deepest first
    }
  }

  return lca;
}

static llvm::Function *extract_node(GumNode *node,
                                    const ExtractOptions &options) {
  if (llvm::Region *region = node->region)
    return extract(region, options);

  return extract({ node->block }, options);
}

DiffResult diff(llvm::Function *src, llvm::Function *dst, DiffOptions options) {
  DiffResult result;

  // Fetch the region information.
  llvm::RegionInfo *src_regions = regions(src), *dst_regions = regions(dst);

  // Construct the gumtree.
  GumTree tree(src,
               dst,
               src_regions,
               dst_regions,
               options.refine_top_down,
               options.match_threshold);

  // Output the GumTree, if requested.
  if (options.dump_gumtree) {
    println("---- SRC TREE (", src->getName(), ") ----");
    print(tree.src);
    println("----");

    println("---- DST TREE (", dst->getName(), ") ----");
    print(tree.dst);
    println("----");
  }

  // If either of the roots are unmatched, do nothing.
  // We need to validate the whole function!
  if (not tree.src->match or not tree.dst->match)
    return result;

  // Collect all dirty nodes.
  llvm::SmallVector<GumNode *> dirty = collect_dirty(tree.src, tree.dst);

  // Find the lowest common (matched) ancestor of all dirty nodes.
  GumNode *lca = find_lca(tree.src, dirty);

  { // Debug print.
    debugln("==== LOWEST COMMON MATCHED ANCESTORS ====");

    debugln("---- SRC TREE (", src->getName(), ") ----");
    debug(lca);
    debugln("----");

    debugln("---- DST TREE (", dst->getName(), ") ----");
    debug(lca->match);
    debugln("----");
    debugln("====");
  }

  // Now that we have the matched ancestors, extract them for validation.
  GumNode *current = lca;
  while (current and current != tree.src) {
    GumNode *match = current->match;
    NIFTY_ASSERT(match, "No match for LCA!");

    // Extract the regions.
    ExtractOptions options;
    llvm::Function *src_func = extract_node(current, options),
                   *dst_func = extract_node(match, options);

    // Ensure that they were both created.
    NIFTY_ASSERT(src_func, "failed to extract src function");
    NIFTY_ASSERT(dst_func, "failed to extract dst function");

    // Record the pair.
    result[src_func] = dst_func;

    { // Debug print
      println("---- SRC LCA FUNCTION ----");
      println(*src_func);
      println("----");
      println("---- DST LCA FUNCTION ----");
      println(*dst_func);
      println("----");
    }

    // Walk up the tree.
    current = current->parent;
  }

  return result;
}

} // namespace nifty
