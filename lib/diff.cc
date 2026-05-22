#include "nifty/diff.hh"
#include "nifty/assert.hh"
#include "nifty/gumtree.hh"
#include "nifty/regions.hh"

namespace nifty {

// ====---- GumTree Diff ----==== //
struct GumDiff {
  llvm::SmallVector<GumNode *> added;
  llvm::SmallVector<GumNode *> removed;
  /** Matched, but body changed */
  llvm::SmallVector<GumNode *> modified;
  /** Matched, but parent changed */
  llvm::SmallVector<GumNode *> moved;
};

GumDiff compute_diff(GumNode *src, GumNode *dst, const GumMatches &matches) {
  GumDiff diff;

  debugln("==== Compute Diff ====");

  for (GumNode *s : src->postorder()) {
    // See if there was a match.
    auto *d = matches.lookup_or(s, nullptr);

    // No match => Removed
    if (not d) {
      diff.removed.push_back(s);
      continue;
    }

    // Mismatched labels => Modified
    if (s->label != d->label)
      diff.modified.push_back(s);

    // Mismatched parents => Moved
    if (s->parent and d->parent                    // both have parents
        and matches.contains(s->parent)            // src parent has a match
        and matches.lookup(s->parent) != d->parent // mismatch
    ) {
      diff.moved.push_back(s);
    }
  }

  for (GumNode *d : dst->postorder()) {
    // Unmatched => Added
    if (not d->match)
      diff.added.push_back(d);
  }

  return diff;
}

// ====---- Diff Driver ----==== //
DiffResult diff(llvm::Function *src, llvm::Function *dst, DiffOptions options) {
  DiffResult result;

  // Construct the gumtree.
  GumTree tree(src, dst, options.refine_top_down, options.match_threshold);

  // Compute the difference.
  GumDiff diff = compute_diff(tree.src, tree.dst, tree.matches);

  // Output the GumTree, if requested.
  if (options.dump_gumtree) {
    println("---- SRC TREE ----");
    print(tree.src);
    println("----");

    println("---- DST TREE ----");
    print(tree.dst);
    println("----");
  }

  { // Debug print.
    debugln("---- ADDED ----");
    for (GumNode *node : diff.added)
      debug(node);
    debugln("----");

    debugln("---- REMOVED ----");
    for (GumNode *node : diff.removed)
      debug(node);
    debugln("----");

    debugln("---- MODIFIED ----");
    for (GumNode *node : diff.modified)
      debug(node);
    debugln("----");

    debugln("---- MOVED ----");
    for (GumNode *node : diff.moved)
      debug(node);
    debugln("----");
  }

  return result;
}

} // namespace nifty
