#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/RegionInfo.h>

#include "nifty/assert.hh"
#include "nifty/diff.hh"
#include "nifty/regions.hh"

namespace nifty {

// ====---- GumTree Helpers ----==== //
static unsigned count_direct_blocks(llvm::Region *region) {
  unsigned count = 0;

  llvm::RegionInfo *info = region->getRegionInfo();
  for (llvm::BasicBlock *block : region->blocks()) {
    // Check if this block belongs to a subregion.
    if (info->getRegionFor(block) != region)
      ++count;
  }

  return count;
}

// ====---- GumTree Algorithm ----==== //
struct BlockLabel {
public:
  const uint64_t hash;

  BlockLabel(llvm::BasicBlock *block) : hash(_compute_hash(block)) {}
  BlockLabel(llvm::Region *region) : hash(_compute_hash(region)) {}

  operator uint64_t() const {
    return this->hash;
  }

private:
  static uint64_t _hash_add(uint64_t h, uint64_t v) {
    return h * 31 + v;
  }

  static uint64_t _compute_hash(llvm::BasicBlock *block) {
    uint64_t h = 0;
    for (llvm::Instruction &inst : *block)
      _hash_add(h, inst.getOpcode());
    for (llvm::Instruction &inst : *block)
      _hash_add(h, uint64_t(inst.getType()));
    _hash_add(h, block->getTerminator()->getNumSuccessors());
    _hash_add(h, block->isEntryBlock());
    // TODO: add PST-specific components
    return h;
  }

  static uint64_t _compute_hash(llvm::Region *region) {
    uint64_t h = _compute_hash(region->getEntry());
    _hash_add(h, count_direct_blocks(region));
    _hash_add(h, std::distance(region->begin(), region->end()));
    return h;
  }
};

struct GumNode {
  // Node information
  llvm::BasicBlock *block;
  BlockLabel label;
  uint64_t subtree_hash = 0;
  unsigned height = 0;
  int postorder_index = 0; // for ordering during maching

  // Tree connections
  GumNode *parent = nullptr;
  llvm::SmallVector<GumNode *> children;

  // Matching state
  GumNode *match = nullptr;

  /** Construct a node for the basic block */
  GumNode(llvm::BasicBlock *block) : block{ block }, label(block), children{} {}
  GumNode(llvm::Region *region)
    : block{ region->getEntry() },
      label(region),
      children{} {}

  llvm::raw_ostream &print(llvm::raw_ostream &os, unsigned indent = 0) {
    std::string pad(indent * 2, ' ');

    os << pad;

    // BB name or number
    if (this->block) {
      os << "BB[";
      this->block->printAsOperand(os, false);
      os << "]";
    } else {
      os << "BB[?]";
    }

    // Is this a leaf (no children = plain BB) or an internal this (region)?
    if (this->children.empty())
      os << " (leaf)";
    else
      os << " (region, " << this->children.size() << " children)";

    // Label summary
    os << " hash=" << llvm::format_hex(uint64_t(this->label), 10);

    // Structural
    os << " height=" << this->height;
    os << " subhash=" << llvm::format_hex(this->subtree_hash, 10);

    // Match status
    if (this->match) {
      os << " matched=>";
      this->match->block->printAsOperand(os, false);
    } else {
      os << " unmatched";
    }

    os << "\n";

    // Recurse into children
    for (GumNode *child : this->children)
      child->print(os, indent + 1);

    return os;
  }

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &, GumNode *);
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, GumNode *node) {
  if (not node) {
    return os << "NULL\n";
  }
  return node->print(os);
}

llvm::SmallVector<GumNode *> postorder(GumNode *root) {
  llvm::SmallVector<GumNode *> result;

  // RPO traversal.
  llvm::SmallVector<GumNode *> stack;
  stack.push_back(root);

  while (not stack.empty()) {
    GumNode *node = stack.pop_back_val();
    result.push_back(node);

    for (GumNode *child : node->children) {
      stack.push_back(child);
    }
  }

  // Reverse to get the postorder.
  std::reverse(result.begin(), result.end());

  return result;
}

// -------- GumTree Construction -------- //
GumNode *build_region(
    llvm::Region *region,
    const llvm::DenseMap<llvm::BasicBlock *, unsigned> &rpo_index) {

  // Create a node for this region.
  auto *node = new GumNode(region);

  { // Build the children.

    // We will build the children first, and reorder them later.
    llvm::SmallVector<std::pair<GumNode *, unsigned>> ordered_children;

    // Collect direct children, treating subregions as opaque.
    llvm::RegionInfo *info = region->getRegionInfo();
    for (llvm::BasicBlock *block : region->blocks()) {
      unsigned index;
      GumNode *child;

      llvm::Region *sub_region = info->getRegionFor(block);
      if (sub_region != region) {
        index = rpo_index.lookup(sub_region->getEntry());
        child = build_region(sub_region, rpo_index);
      } else {
        index = rpo_index.lookup(block);
        child = new GumNode(block);
      }

      // Register the parent-child relationship.
      child->parent = node;
      ordered_children.emplace_back(child, index);
    }

    // Sort the children by RPO index of their entry block.
    llvm::sort(ordered_children,
               [](auto &a, auto &b) { return a.second < b.second; });

    // Output the children in order.
    for (auto &[child, _index] : ordered_children)
      node->children.push_back(child);
  }

  { // Compute height and subtree hash bottom-up.
    if (node->children.empty()) {
      node->height = 0;
      node->subtree_hash = node->label;
    } else {
      node->height =
          1 + (*llvm::max_element(node->children, [](GumNode *a, GumNode *b) {
                return a->height < b->height;
              }))->height;
      uint64_t h = node->label;
      for (GumNode *child : node->children)
        h = h * 131 + child->subtree_hash;
      node->subtree_hash = h;
    }
  }

  return node;
}

GumNode *build_tree(llvm::Function *function, llvm::RegionInfo *regions) {
  println("==== Build Tree ====");
  llvm::ReversePostOrderTraversal<llvm::Function *> rpo_traversal(function);

  llvm::DenseMap<llvm::BasicBlock *, unsigned> rpo_index;
  for (auto [index, block] : llvm::enumerate(rpo_traversal))
    rpo_index[block] = index;

  return build_region(regions->getTopLevelRegion(), rpo_index);
}

// -------- GumTree Top-down -------- //
const unsigned MIN_HEIGHT = 1;

using Matches = typename llvm::DenseMap<GumNode *, GumNode *>;

void match_subtree(GumNode *src, GumNode *dst, Matches &matches) {
  NIFTY_ASSERT(src->subtree_hash == dst->subtree_hash,
               "Mismatched subtree hash!");
  // Record the match.
  matches[src] = dst;
  // Record the match in the nodes.
  dst->match = src;
  src->match = dst;
  // Recurse on children.
  for (auto [sc, dc] : llvm::zip(src->children, dst->children))
    match_subtree(sc, dc, matches);
}

GumNode *prev_sibling(GumNode *node) {
  if (not node->parent)
    return nullptr;
  auto &siblings = node->parent->children;
  auto it = llvm::find(siblings, node);
  if (it == siblings.begin())
    return nullptr;
  return *std::prev(it);
}

double ancestor_dice(GumNode *src, GumNode *can, Matches &matches) {
  unsigned matched = 0, total = 0;
  GumNode *s = src->parent;
  GumNode *c = can->parent;

  while (s and c) {
    auto it = matches.find(s);
    if (it != matches.end() and it->second == c)
      ++matched;
    ++total;

    s = s->parent;
    c = c->parent;
  }

  if (total == 0)
    return 0.0;

  return double(matched) / total;
}

GumNode *best_candidate(GumNode *src,
                        llvm::SmallVector<GumNode *> &candidates,
                        Matches &matches) {
  GumNode *best = nullptr;
  double best_score = -1.0;

  for (GumNode *can : candidates) {
    double score = 0.0;

    // Signal 1: parent is already matched to src's parent
    // This is the strongest signal, same position in tree
    if (src->parent and can->parent) {
      auto it = matches.find(src->parent);
      if (it != matches.end() and it->second == can->parent)
        score += 2.0;
    }

    // Signal 2: left sibling is already matched to src's left sibling
    // Gives positional ordering within a parent's children
    GumNode *src_prev_sibling = prev_sibling(src),
            *can_prev_sibling = prev_sibling(can);
    if (src_prev_sibling and can_prev_sibling) {
      auto it = matches.find(src_prev_sibling);
      if (it != matches.end() and it->second == can_prev_sibling)
        score += 1.0;
    }

    // Signal 3: dice similarity of already-matched ancestors
    // Handles the case where neither parent nor sibling is matched yet
    score += 0.5 * ancestor_dice(src, can, matches);

    if (score > best_score) {
      best_score = score;
      best = can;
    }
  }

  // If candidate had a positive score, return it.
  if (best_score > 0.0)
    return best;

  // Otherwise, return NULL to try smaller subtrees.
  return nullptr;
}

void top_down(GumNode *src, GumNode *dst, Matches &matches) {
  println("==== Top-Down ====");

  // Max-priority queue ordered by node height
  // Each entry is a list of nodes at that height
  // NOTE: This function assumes that iterators are stable under erasure.
  using MaxPrioQueue =
      std::map<unsigned, llvm::SmallVector<GumNode *>, std::greater<unsigned>>;
  MaxPrioQueue src_queue, dst_queue;

  // Helper function to push node at its height.
  auto push_open = [](GumNode *n, MaxPrioQueue &q) {
    q[n->height].push_back(n);
  };

  push_open(src, src_queue);
  push_open(dst, dst_queue);

  while (not src_queue.empty() and not dst_queue.empty()) {
    auto src_begin = src_queue.begin(), dst_begin = dst_queue.begin();
    unsigned src_max = src_begin->first, dst_max = dst_begin->first;

    println("src.max = ", src_max, "    dst.max = ", dst_max);

    // Always process the taller side down to match heights.
    if (src_max != dst_max) {
      if (src_max > dst_max) {
        for (auto *n : src_begin->second)
          for (auto *c : n->children)
            push_open(c, src_queue);
        src_queue.erase(src_begin);
      } else {
        for (auto *n : dst_begin->second)
          for (auto *c : n->children)
            push_open(c, dst_queue);
        dst_queue.erase(dst_begin);
      }
      continue;
    }

    // Fetch the nodes at the shared height.
    auto &src_nodes = src_begin->second, &dst_nodes = dst_begin->second;

    println("# SRC NODES ", src_nodes.size());
    println("# DST NODES ", dst_nodes.size());

    // Group dst nodes by subtree hash for fast lookup.
    llvm::DenseMap<uint64_t, llvm::SmallVector<GumNode *>> dst_by_hash;
    for (GumNode *n : dst_nodes) {
      dst_by_hash[n->subtree_hash].push_back(n);
    }

    // Heights are equal, try to match by subtree hash.
    for (GumNode *s : src_nodes) {
      auto it = dst_by_hash.find(s->subtree_hash);
      if (it == dst_by_hash.end()) {
        // No match at this height, push children to try smaller subtrees.
        for (auto *c : s->children)
          push_open(c, src_queue);
        continue;
      }

      auto &candidates = it->second;

      // Unique match at this height, commit entire isomorphic subtree.
      if (candidates.size() == 1) {
        match_subtree(s, candidates.front(), matches);
        continue;
      }

      // Ambiguous, find best candidate by position similarity.
      GumNode *best = best_candidate(s, candidates, matches);
      if (best) {
        match_subtree(s, best, matches);
        continue;
      }

      // No match at this height, push children to try smaller subtrees.
      for (auto *c : s->children)
        push_open(c, src_queue);
    }

    // Dequeue this height.
    src_queue.erase(src_begin);
    dst_queue.erase(dst_begin);
  }
}

// ====---- GumTree Bottom-up ----===== //
static void _descendants(llvm::SmallVector<GumNode *> &result, GumNode *node) {
  for (GumNode *child : node->children) {
    result.push_back(child);
    _descendants(result, node);
  }
}

static llvm::SmallVector<GumNode *> descendants(GumNode *node) {
  llvm::SmallVector<GumNode *> result;
  _descendants(result, node);
  return result;
}

static bool is_descendant(GumNode *node, GumNode *root) {
  GumNode *cur = node->parent;
  while (cur) {
    if (cur == root)
      return true;
    cur = cur->parent;
  }
  return false;
}

double dice(GumNode *src, GumNode *dst, const Matches &matches) {
  llvm::SmallVector<GumNode *> src_descendants = descendants(src),
                               dst_descendants = descendants(dst);

  unsigned total = src_descendants.size() + dst_descendants.size();
  if (total == 0)
    return 0.0;

  unsigned common = 0;
  for (auto *desc : src_descendants) {
    auto it = matches.find(desc);
    if (it != matches.end())
      if (is_descendant(it->second, dst))
        ++common;
  }

  return 2.0 * common / total;
}

const double DICE_THRESHOLD = 5.0;

void bottom_up(GumNode *src,
               GumNode *dst,
               Matches &matches,
               bool refine_top_down = false) {
  println("==== Bottom-Up ====");

  for (GumNode *s : postorder(src)) {
    // Already matched.
    if (matches.contains(s))
      continue;
    // Ignore leaf nodes.
    if (s->children.empty())
      continue;

    // Find best candidate in dst by dice similarity.
    GumNode *best = nullptr;
    double best_dice = 0.0;

    for (GumNode *d : postorder(dst)) {
      if (matches.contains(d))
        continue;
      if (d->label != s->label)
        continue;

      if (is_descendant(s, d)) {
        double d_dice = dice(s, d, matches);
        if (d_dice > DICE_THRESHOLD and d_dice > best_dice) {
          best_dice = d_dice;
          best = d;
        }
      }
    }

    if (best) {
      matches[s] = best;
      best->match = s;
      s->match = best;

      // Optionally, recover any missed subtree matches.
      if (refine_top_down)
        top_down(s, best, matches);
    }
  }
}

// ====---- GumTree Diff ----==== //
struct GumDiff {
  llvm::SmallVector<GumNode *> added;
  llvm::SmallVector<GumNode *> removed;
  /** Matched, but body changed */
  llvm::SmallVector<GumNode *> modified;
  /** Matched, but parent changed */
  llvm::SmallVector<GumNode *> moved;
};

GumDiff compute_diff(GumNode *src, GumNode *dst, const Matches &matches) {
  GumDiff diff;

  println("==== Compute Diff ====");

  for (GumNode *s : postorder(src)) {
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

  for (GumNode *d : postorder(dst)) {
    // Unmatched => Added
    if (not d->match)
      diff.added.push_back(d);
  }

  return diff;
}

// ====---- Diff Driver ----==== //
DiffResult diff(llvm::Function *src, llvm::Function *dst, DiffOptions options) {
  DiffResult result;

  // Fetch the region information.
  llvm::RegionInfo *src_regions = regions(src), *dst_regions = regions(dst);

  // Build GumTree
  GumNode *src_tree = build_tree(src, src_regions),
          *dst_tree = build_tree(dst, dst_regions);

  println("---- SRC TREE ----");
  print(dst_tree);
  println("----");

  println("---- DST TREE ----");
  print(src_tree);
  println("----");

  // Run GumTree algorithm
  Matches matches;
  top_down(src_tree, dst_tree, matches);
  bottom_up(src_tree, dst_tree, matches);

  // Compute the difference.
  GumDiff diff = compute_diff(src_tree, dst_tree, matches);

  // Debug print.
  if (options.dump_gumtree) {
    println("---- SRC TREE ----");
    print(dst_tree);
    println("----");

    println("---- DST TREE ----");
    print(src_tree);
    println("----");
  }

  return result;
}

} // namespace nifty
