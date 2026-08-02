#include <llvm/Analysis/RegionInfo.h>

#include "nifty/assert.hh"
#include "nifty/diff.hh"
#include "nifty/extract.hh"
#include "nifty/gumtree.hh"
#include "nifty/regions.hh"
#include "nifty/constraints.hh"

#include <chrono>
#include <iostream>

// MACROS
#define clock_now() high_resolution_clock::now()
#define compute_duration()                                                     \
  duration_cast<microseconds>(end - start).count() / 1000000.0

using namespace std::chrono;

namespace nifty {

static void collect_dirty_src(llvm::SmallVector<GumNode *> &dirty,
                              GumNode *node) {
  // Prevent duplicates in the vector
  if (!node->dirty) {
    if (not node->match) {
      // unmatched => added or removed
      dirty.push_back(node);
      node->dirty = true;

      // The parent MUST be dirty
      if (node->parent && !node->parent->dirty) {
        dirty.push_back(node->parent);
        node->parent->dirty = true;
      }
    } else if (node->label != node->match->label) {
      // matched, but different hash => modified.
      dirty.push_back(node);
      node->dirty = true;
    }
  }

  // Recurse on children.
  for (GumNode *child : node->children)
    collect_dirty_src(dirty, child);
}

static void collect_dirty_dst(llvm::SmallVector<GumNode *> &dirty,
                              GumNode *node) {
  // Found an unmatched node.
  if (not node->match) {
    node->dirty = true;

    // Walk up to find the lowest matched ancestor in dst, then use its src
    // match as the dirty anchor.
    GumNode *current = node->parent;
    while (current and not current->match) {
      current->dirty = true;
      current = current->parent;
    }

    if (current && !current->match->dirty) {
      dirty.push_back(current->match);
      current->match->dirty = true;
    }
  } else if (node->label != node->match->label) {
    node->dirty = true;
    // Redundant check: the  matched node must be dirty
    if (!node->match->dirty) {
      node->match->dirty = true;
      dirty.push_back(node->match);
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
  if (lca->is_instr) {
    std::cerr << "---- LCA IS AN INSTRUCTION ----\n";
    std::cerr << "---- GOING UP THE TREE FOR A BLOCK ----\n";
    return lca->parent;
  }
  return lca;
}

static void subtree_sizes(GumNode *src_root,
                          llvm::DenseMap<GumNode *, unsigned> *size) {
  unsigned s = 1;
  for (GumNode *n : src_root->children) {
    subtree_sizes(n, size);
    s += (*size)[n];
  }
  (*size)[src_root] = s;
  return;
}

static void dirty_subtree_sizes(GumNode *src_root,
                                llvm::DenseMap<GumNode *, unsigned> *size,
                                const llvm::SmallVector<GumNode *> &dirty) {
  for (GumNode *node : dirty) {
    GumNode *current = node;
    while (current) {
      ++((*size)[current]);
      current = current->parent;
    }
  }
  return;
}
static llvm::Function *extract_node(GumNode *node,
                                    const ExtractOptions &options) {
  if (llvm::Region *region = node->region) return extract(region, options);
  if (!node->is_block) std::cerr << "Passed node is not a block\n";

  return extract({node->block}, options);
}

static bool same_signature(llvm::Function *src, llvm::Function *dst) {
  // Check that they have the same function type.
  if (src->getType() != dst->getType()) return false;

  // Check that they have the same calling convention.
  if (src->getCallingConv() != dst->getCallingConv()) return false;

  // Check that they have the same attributes.
  if (src->getAttributes() != dst->getAttributes()) return false;

  return true;
}

DiffResult diff(llvm::Function *src, llvm::Function *dst, DiffOptions options) {
  // COCKA2 TODO: potentially different initialization for DiffResult, where
  // DiffStats are being passed from the caller
  DiffResult result;
  DiffStats *diff_stats = result.diff_stats;
  std::list<GumNode> gumnodes = {};

  // So that no initialization errors are emitted down the line
  auto start = high_resolution_clock::now();
  auto end = high_resolution_clock::now();
  double duration = 0.0;

  // Check the function signature for differences.
  if (not same_signature(src, dst)) return result;

  // Add constraints
  ConstraintOptions constr_options;
  if(infer_constraints(src, constr_options))
    println("---- ASSUMPTIONS INSERTED INTO SRC FUNCTION ----");
  else
    println("---- ASSUMPTIONS NOT INSERTED INTO SRC FUNCTION ----");
  if(infer_constraints(dst, constr_options))
    println("---- ASSUMPTIONS INSERTED INTO DST FUNCTION ----");
  else
    println("---- ASSUMPTIONS NOT INSERTED INTO DST FUNCTION ----");
  // Fetch the region information.
  llvm::RegionInfo *src_regions = regions(src), *dst_regions = regions(dst);

  // Construct the gumtree.

  GumTree tree(src, dst, src_regions, dst_regions, options.refine_top_down,
               options.match_threshold, diff_stats, &gumnodes);

  // If either of the roots are unmatched, do nothing.
  // We need to validate the whole function!
  if (not tree.src->match or not tree.dst->match) {
    println("EARLY RETURN, roots do not match");
    return result;
  }

  // Collect all dirty nodes.

  start = high_resolution_clock::now();
  llvm::SmallVector<GumNode *> dirty = collect_dirty(tree.src, tree.dst);
  end = high_resolution_clock::now();
  duration = duration_cast<microseconds>(end - start).count() / 1000000.0;
  diff_stats->dirty_duration = duration;

  // Output the GumTree, if requested.
  if (options.dump_gumtree) {
    println("---- SRC TREE (", src->getName(), ") ----");
    print(tree.src);
    println("----");

    println("---- DST TREE (", dst->getName(), ") ----");
    print(tree.dst);
    println("----");

    /*


    debugln("---- DST WHOLE FUNCTION (", dst->getName(), ") ----");
    debugln("---------- LOOP before dst --------");
    debugln(*dst_func);
    debugln("---------- LOOP after dst --------");
    debugln("-----------------------");
    */
  }

  // If there are no dirty nodes, then we don't need to extract any nodes.
  if (dirty.empty()) {
    println("EARLY RETURN, no dirty nodes");
    return result;
  }

  // Find the lowest common (matched) ancestor of all dirty nodes.
  start = high_resolution_clock::now();
  GumNode *lca = find_lca(tree.src, dirty);
  end = high_resolution_clock::now();

  duration = duration_cast<microseconds>(end - start).count() / 1000000.0;
  diff_stats->find_lca_duration = duration;

  tree.compute_lca_map(dirty, diff_stats);
  // Computing subtree/dirty subtree sizes for threshold computations

  llvm::DenseMap<GumNode *, unsigned> size;
  llvm::DenseMap<GumNode *, unsigned> dirty_size;

  subtree_sizes(tree.src, &size);
  dirty_subtree_sizes(tree.src, &dirty_size, dirty);

  {
    diff_stats->lca_computed = true;
    diff_stats->lca_dirty = lca->dirty;
    diff_stats->lca_height = lca->height;
    diff_stats->lca_subtree_size = size[lca];
    diff_stats->lca_dirty_subtree_size = dirty_size[lca];
  }
  /*
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
  */
  // Now that we have the matched ancestors, extract them for validation.
  GumNode *current = lca;
  // TOREMOVE
  // debugln("Subtree size: ", size[lca]);
  // debugln("Dirty subtree size: ", dirty_size[lca]);

  {
    if (current && current != tree.src &&
        !(current->parent == tree.src && tree.src->children.size() == 1)) {
      println("---- LCA IS NOT THE WHOLE FUNCTION ----");
      if (!lca->region && !lca->instr)
        println("---- LCA BLOCK NAME IS ", lca->block->getNameOrAsOperand());
    } else {
      println("---- LCA IS THE WHOLE FUNCTION ----");
    }
  }

  // TOREMOVE
  while (current and current != tree.src) {
    GumNode *match = current->match;
    NIFTY_ASSERT(match, "No match for LCA!");

    // Extract the regions.
    ExtractOptions options;

    double duration_src, duration_tgt;
    start = clock_now();
    llvm::Function *src_func = extract_node(current, options);
    end = clock_now();
    duration_src = compute_duration();
    diff_stats->extract_node_src_duration.push_back(duration_src);

    start = clock_now();
    llvm::Function *dst_func = extract_node(match, options);
    end = clock_now();
    duration_tgt = compute_duration();
    diff_stats->extract_node_tgt_duration.push_back(duration_tgt);
    diff_stats->extract_node_duration.push_back(duration_src + duration_tgt);

    // Ensure that they were both created.
    NIFTY_ASSERT(src_func, "failed to extract src function");
    NIFTY_ASSERT(dst_func, "failed to extract dst function");

    // Record the pair.
    result.pairs[src_func] = dst_func;
    result.diff_stats->pairs_heights.emplace_back(match->height);

    /*
    { // Debug print
      debugln("---- SRC LCA FUNCTION ----");
      debugln(*src_func);
      debugln("----");
      debugln("---- DST LCA FUNCTION ----");
      debugln(*dst_func);
      debugln("----");
    }
    */

    // Walk up the tree.
    current = current->parent;
  }

  return result;
}

} // namespace nifty
