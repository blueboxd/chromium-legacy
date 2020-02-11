// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/substring_set_matcher.h"

#include <stddef.h>

#include <algorithm>
#include <queue>

#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace url_matcher {

namespace {

// Compare StringPattern instances based on their string patterns.
bool ComparePatterns(const StringPattern* a, const StringPattern* b) {
  return a->pattern() < b->pattern();
}

// Given the set of patterns, compute how many nodes will the corresponding
// Aho-Corasick tree have. Note that |patterns| need to be sorted.
uint32_t TreeSize(const std::vector<const StringPattern*>& patterns) {
  DCHECK(std::is_sorted(patterns.begin(), patterns.end(), ComparePatterns));

  uint32_t result = 1u;  // 1 for the root node.
  if (patterns.empty())
    return result;

  auto last = patterns.begin();
  auto current = last + 1;
  // For the first pattern, each letter is a label of an edge to a new node.
  result += (*last)->pattern().size();

  // For the subsequent patterns, only count the edges which were not counted
  // yet. For this it suffices to test against the previous pattern, because the
  // patterns are sorted.
  for (; current != patterns.end(); ++last, ++current) {
    const std::string& last_pattern = (*last)->pattern();
    const std::string& current_pattern = (*current)->pattern();
    const uint32_t prefix_bound =
        std::min(last_pattern.size(), current_pattern.size());

    uint32_t common_prefix = 0;
    while (common_prefix < prefix_bound &&
           last_pattern[common_prefix] == current_pattern[common_prefix])
      ++common_prefix;
    result += current_pattern.size() - common_prefix;
  }
  return result;
}

std::vector<const StringPattern*> GetVectorOfPointers(
    const std::vector<StringPattern>& patterns) {
  std::vector<const StringPattern*> pattern_pointers;
  pattern_pointers.reserve(patterns.size());

  for (const StringPattern& pattern : patterns)
    pattern_pointers.push_back(&pattern);

  return pattern_pointers;
}

}  // namespace

SubstringSetMatcher::SubstringSetMatcher(
    const std::vector<StringPattern>& patterns)
    : SubstringSetMatcher(GetVectorOfPointers(patterns)) {}

SubstringSetMatcher::SubstringSetMatcher(
    std::vector<const StringPattern*> patterns) {
  // Ensure there are no duplicate IDs and all pattern strings are distinct.
#if DCHECK_IS_ON()
  {
    std::set<StringPattern::ID> ids;
    std::set<std::string> pattern_strings;
    for (const StringPattern* pattern : patterns) {
      CHECK(!base::Contains(ids, pattern->id()));
      CHECK(!base::Contains(pattern_strings, pattern->pattern()));
      ids.insert(pattern->id());
      pattern_strings.insert(pattern->pattern());
    }
  }
#endif

  // Compute the total number of tree nodes needed.
  std::sort(patterns.begin(), patterns.end(), ComparePatterns);
  tree_.reserve(TreeSize(patterns));
  BuildAhoCorasickTree(patterns);

  // Sanity check that no new allocations happened in the tree and our computed
  // size was correct.
  DCHECK_EQ(tree_.size(), TreeSize(patterns));

  is_empty_ = patterns.empty() && tree_.size() == 1u;
}

SubstringSetMatcher::~SubstringSetMatcher() = default;

bool SubstringSetMatcher::Match(const std::string& text,
                                std::set<StringPattern::ID>* matches) const {
  const size_t old_number_of_matches = matches->size();

  // Handle patterns matching the empty string.
  const AhoCorasickNode* const root = &tree_[0];
  root->AccumulateMatches(matches);

  const AhoCorasickNode* current_node = root;
  for (const char c : text) {
    AhoCorasickNode* child = current_node->GetEdge(c);

    // If the child not can't be found, progressively iterate over the longest
    // proper suffix of the string represented by the current node. In a sense
    // we are pruning prefixes from the text.
    while (!child && current_node != root) {
      current_node = current_node->failure();
      child = current_node->GetEdge(c);
    }

    if (child) {
      // The string represented by |child| is the longest possible suffix of the
      // current position of |text| in the trie.
      current_node = child;
      current_node->AccumulateMatches(matches);
    } else {
      // The empty string is the longest possible suffix of the current position
      // of |text| in the trie.
      DCHECK_EQ(root, current_node);
    }
  }

  return old_number_of_matches != matches->size();
}

size_t SubstringSetMatcher::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(tree_);
}

void SubstringSetMatcher::BuildAhoCorasickTree(
    const SubstringPatternVector& patterns) {
  DCHECK(tree_.empty());

  // Initialize root node of tree.
  // Sanity check that there's no reallocation on adding a new node since we
  // take pointers to nodes in the |tree_|, which can be invalidated in case of
  // a reallocation.
  DCHECK_LT(tree_.size(), tree_.capacity());
  tree_.emplace_back();

  // Build the initial trie for all the patterns.
  for (const StringPattern* pattern : patterns)
    InsertPatternIntoAhoCorasickTree(pattern);

  // Trie creation is complete and edges are finalized. Shrink to fit each edge
  // map to save on memory.
  for (AhoCorasickNode& node : tree_)
    node.ShrinkEdges();

  CreateFailureAndOutputEdges();
}

void SubstringSetMatcher::InsertPatternIntoAhoCorasickTree(
    const StringPattern* pattern) {
  const std::string& text = pattern->pattern();
  const std::string::const_iterator text_end = text.end();

  // Iterators on the tree and the text.
  AhoCorasickNode* current_node = &tree_[0];
  std::string::const_iterator i = text.begin();

  // Follow existing paths for as long as possible.
  while (i != text_end) {
    AhoCorasickNode* child = current_node->GetEdge(*i);
    if (!child)
      break;
    current_node = child;
    ++i;
  }

  // Create new nodes if necessary.
  while (i != text_end) {
    // Sanity check that there's no reallocation on adding a new node since we
    // take pointers to nodes in the |tree_|, which can be invalidated in case
    // of a reallocation.
    DCHECK_LT(tree_.size(), tree_.capacity());
    tree_.emplace_back();

    AhoCorasickNode* child = &tree_.back();
    current_node->SetEdge(*i, child);
    current_node = child;
    ++i;
  }

  // Register match.
  current_node->SetMatchID(pattern->id());
}

void SubstringSetMatcher::CreateFailureAndOutputEdges() {
  base::queue<AhoCorasickNode*> queue;

  // Initialize the failure edges for |root| and its children.
  AhoCorasickNode* const root = &tree_[0];

  // Assigning |root| as the failure edge for itself doesn't strictly abide by
  // the definition of "proper" suffix. The proper suffix of an empty string
  // should probably be defined as null, but we assign it to the |root| to
  // simplify the code and have the invariant that the failure edge is always
  // defined.
  root->SetFailure(root);

  root->SetOutputLink(nullptr);

  const AhoCorasickNode* const root_output_link =
      root->IsEndOfPattern() ? root : nullptr;
  for (const auto& edge : root->edges()) {
    AhoCorasickNode* child = edge.second;
    child->SetFailure(root);
    child->SetOutputLink(root_output_link);
    queue.push(child);
  }

  // Do a breadth first search over the trie to create failure edges. We
  // maintain the invariant that any node in |queue| has had its |failure_| and
  // |output_link_| edge already initialized.
  while (!queue.empty()) {
    AhoCorasickNode* current_node = queue.front();
    queue.pop();

    // Compute the failure and output edges of children using the failure edges
    // of the current node.
    for (const auto& edge : current_node->edges()) {
      const char edge_label = edge.first;
      AhoCorasickNode* child = edge.second;

      const AhoCorasickNode* failure_candidate_parent = current_node->failure();
      const AhoCorasickNode* failure_candidate =
          failure_candidate_parent->GetEdge(edge_label);

      while (!failure_candidate && failure_candidate_parent != root) {
        failure_candidate_parent = failure_candidate_parent->failure();
        failure_candidate = failure_candidate_parent->GetEdge(edge_label);
      }

      if (!failure_candidate) {
        DCHECK_EQ(root, failure_candidate_parent);
        // |failure_candidate| is null and we can't proceed further since we
        // have reached the root. Hence the longest proper suffix of this string
        // represented by this node is the empty string (represented by root).
        failure_candidate = root;
      }

      child->SetFailure(failure_candidate);

      // Now |failure_candidate| is |child|'s longest possible proper suffix in
      // the trie. We also know that since we are doing a breadth first search,
      // we would have established |failure_candidate|'s output link by now.
      // Hence we can define |child|'s output link as follows:
      child->SetOutputLink(failure_candidate->IsEndOfPattern()
                               ? failure_candidate
                               : failure_candidate->output_link());

      queue.push(child);
    }
  }
}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode() = default;
SubstringSetMatcher::AhoCorasickNode::~AhoCorasickNode() = default;

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode(AhoCorasickNode&& other) =
    default;

SubstringSetMatcher::AhoCorasickNode& SubstringSetMatcher::AhoCorasickNode::
operator=(AhoCorasickNode&& other) = default;

SubstringSetMatcher::AhoCorasickNode*
SubstringSetMatcher::AhoCorasickNode::GetEdge(char c) const {
  auto i = edges_.find(c);
  return i == edges_.end() ? nullptr : i->second;
}

void SubstringSetMatcher::AhoCorasickNode::SetEdge(char c,
                                                   AhoCorasickNode* node) {
  DCHECK(node);
  edges_[c] = node;
}

void SubstringSetMatcher::AhoCorasickNode::SetFailure(
    const AhoCorasickNode* node) {
  DCHECK(node);
  failure_ = node;
}

void SubstringSetMatcher::AhoCorasickNode::SetOutputLink(
    const AhoCorasickNode* output_link) {
  DCHECK(!output_link || output_link->IsEndOfPattern());
  output_link_ = output_link;
}

void SubstringSetMatcher::AhoCorasickNode::AccumulateMatches(
    std::set<StringPattern::ID>* matches) const {
  DCHECK(matches);

  if (IsEndOfPattern())
    matches->insert(GetMatchID());

  for (const AhoCorasickNode* node = this->output_link(); !!node;
       node = node->output_link()) {
    matches->insert(node->GetMatchID());
  }
}

size_t SubstringSetMatcher::AhoCorasickNode::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(edges_);
}

}  // namespace url_matcher
