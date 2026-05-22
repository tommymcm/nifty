#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/RegionInfo.h>

#include "nifty/assert.hh"
#include "nifty/diff.hh"
#include "nifty/gumtree.hh"
#include "nifty/regions.hh"

namespace nifty {

// ====---- Hash ----==== //
static const uint64_t SMALL_PRIME = 31;

static inline uint64_t combine_hash(const uint64_t h, const uint64_t v) {
  return h * SMALL_PRIME + v;
}

static uint64_t compute_hash(llvm::BasicBlock *block) {
  uint64_t h = 0;
  for (llvm::Instruction &inst : *block)
    h = combine_hash(h, inst.getOpcode());
  for (llvm::Instruction &inst : *block)
    h = combine_hash(h, uint64_t(inst.getType()));
  h = combine_hash(h, block->getTerminator()->getNumSuccessors());
  h = combine_hash(h, block->isEntryBlock());
  // TODO: add PST-specific components
  return h;
}

static uint64_t compute_hash(llvm::Region *region) {
  return compute_hash(region->getEntry());
}

// ====---- GumNode ----==== //
GumNode::GumNode(llvm::BasicBlock *block)
  : block{ block },
    label{ compute_hash(block) },
    children{} {
  this->subtree_hash = uint64_t(this->label);
}

GumNode::GumNode(llvm::Region *region)
  : block{ region->getEntry() },
    label{ compute_hash(region) },
    children{} {
  this->subtree_hash = uint64_t(this->label);
}

llvm::SmallVector<GumNode *> GumNode::postorder() {
  llvm::SmallVector<GumNode *> result;

  // RPO traversal.
  llvm::SmallVector<GumNode *> stack;
  stack.push_back(this);

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

static llvm::raw_ostream &node_name(llvm::raw_ostream &os, GumNode *node) {
  // Is this a leaf (no children = plain BB) or an internal this (region)?
  os << (node->children.empty() ? "Block" : "Region");

  // BB name or number
  if (node->block) {
    os << "[";
    node->block->printAsOperand(os, false);
    os << "]";
  } else {
    os << "[?]";
  }

  return os;
}

static llvm::raw_ostream &print(llvm::raw_ostream &os,
                                GumNode *node,
                                unsigned indent = 0) {
  std::string pad(indent * 2, ' ');

  os << pad;

  node_name(os, node);

  // Label summary
  os << " hash=" << llvm::format_hex(uint64_t(node->label), 10);

  // Structural
  os << " height=" << node->height;
  os << " subhash=" << llvm::format_hex(node->subtree_hash, 10);

  // Match status
  if (node->match) {
    os << " matched=>";
    node_name(os, node->match);
  } else {
    os << " unmatched";
  }

  os << "\n";

  // Recurse into children
  for (GumNode *child : node->children)
    print(os, child, indent + 1);

  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, GumNode *node) {
  if (not node)
    return os << "NULL\n";
  return print(os, node);
}

// ====---- Initialization ----==== //
static GumNode *build_region(
    llvm::Region *region,
    const llvm::DenseMap<llvm::BasicBlock *, unsigned> &rpo_index) {

  // Create a node for this region.
  auto *node = new GumNode(region);

  { // Build the children.

    // We will build the children first, and reorder them later.
    llvm::SmallVector<std::pair<GumNode *, unsigned>> ordered_children;

    // Build direct subregion children.
    for (std::unique_ptr<llvm::Region> &sub_region : *region) {
      unsigned index = rpo_index.lookup(sub_region->getEntry());
      GumNode *child = build_region(sub_region.get(), rpo_index);
      child->parent = node;
      ordered_children.emplace_back(child, index);
    }

    // Build direct block children.
    llvm::RegionInfo *info = region->getRegionInfo();
    for (llvm::BasicBlock *block : region->blocks()) {
      // Skip subregion entry blocks.
      if (region->getSubRegionNode(block))
        continue;
      // Skip blocks that belong to any subregion other than this one.
      if (info->getRegionFor(block) != region)
        continue;

      unsigned index = rpo_index.lookup(block);
      GumNode *child = new GumNode(block);
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
  debugln("==== Build Tree ====");
  llvm::ReversePostOrderTraversal<llvm::Function *> rpo_traversal(function);

  llvm::DenseMap<llvm::BasicBlock *, unsigned> rpo_index;
  for (auto [index, block] : llvm::enumerate(rpo_traversal))
    rpo_index[block] = index;

  return build_region(regions->getTopLevelRegion(), rpo_index);
}

// ====---- GumTree Top-down ----==== //
static void match_subtree(GumNode *src, GumNode *dst, GumMatches &matches) {
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

static GumNode *prev_sibling(GumNode *node) {
  if (not node->parent)
    return nullptr;
  auto &siblings = node->parent->children;
  auto it = llvm::find(siblings, node);
  if (it == siblings.begin())
    return nullptr;
  return *std::prev(it);
}

static double ancestor_dice(GumNode *src, GumNode *can, GumMatches &matches) {
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

static GumNode *best_candidate(GumNode *src,
                               llvm::SmallVector<GumNode *> &candidates,
                               GumMatches &matches) {
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

static void top_down(GumNode *src, GumNode *dst, GumMatches &matches) {
  debugln("==== Top-Down ====");

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

    debugln("src.max = ", src_max, "    dst.max = ", dst_max);

    // Always process the taller side down to match heights.
    if (src_max != dst_max) {
      auto &taller = src_max > dst_max ? src_queue : dst_queue;
      for (auto *n : taller.begin()->second)
        for (auto *c : n->children)
          push_open(c, taller);
      taller.erase(taller.begin());
      continue;
    }

    // Fetch the nodes at the shared height.
    auto &src_nodes = src_begin->second, &dst_nodes = dst_begin->second;

    debugln("# SRC NODES ", src_nodes.size());
    debugln("# DST NODES ", dst_nodes.size());

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
        GumNode *candidate = candidates.front();
        match_subtree(s, candidate, matches);
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

    // Dst unmatched => push its children
    for (GumNode *d : dst_nodes) {
      if (not d->match)
        for (GumNode *c : d->children)
          push_open(c, dst_queue);
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
    _descendants(result, child);
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

static double dice(GumNode *src, GumNode *dst, const GumMatches &matches) {
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

static void bottom_up(GumNode *src,
                      GumNode *dst,
                      GumMatches &matches,
                      bool refine_top_down = false,
                      double dice_threshold = 0.5) {
  debugln("==== Bottom-Up ====");

  for (GumNode *s : src->postorder()) {
    // Already matched.
    if (s->match)
      continue;

    bool s_is_leaf = s->children.empty();

    // Find best candidate in dst by dice similarity.
    GumNode *best = nullptr;
    double best_dice = 0.0;

    for (GumNode *d : dst->postorder()) {
      // Already matched.
      if (d->match)
        continue;
      // Local information does not match.
      if (d->label != s->label)
        continue;

      // Leaves trivially match.
      bool d_is_leaf = d->children.empty();
      if (s_is_leaf and d_is_leaf) {
        best = d;
        break;
      }

      // Internal nodes match on dice threshold.
      double d_dice = dice(s, d, matches);
      if (d_dice >= dice_threshold and d_dice > best_dice) {
        best_dice = d_dice;
        best = d;
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

// ====---- GumTree ----==== //
GumTree::GumTree(llvm::Function *src,
                 llvm::Function *dst,
                 llvm::RegionInfo *src_regions,
                 llvm::RegionInfo *dst_regions,
                 bool refine_top_down,
                 double match_threshold)
  : matches{} {

  // Build GumTree.
  this->src = build_tree(src, src_regions);
  this->dst = build_tree(dst, dst_regions);

  // Match GumTree.
  top_down(this->src, this->dst, this->matches);
  bottom_up(this->src,
            this->dst,
            this->matches,
            refine_top_down,
            match_threshold);
}

} // namespace nifty
