// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_
#define COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_

#include <stdint.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/url_matcher/string_pattern.h"
#include "components/url_matcher/url_matcher_export.h"

namespace url_matcher {

// Class that store a set of string patterns and can find for a string S,
// which string patterns occur in S.
class URL_MATCHER_EXPORT SubstringSetMatcher {
 public:
  // Registers all |patterns|. Each pattern needs to have a unique ID and all
  // pattern strings must be unique.
  // Complexity:
  //    Let n = number of patterns.
  //    Let S = sum of pattern lengths.
  //    Let k = range of char. Generally 256.
  // Complexity = O(nlogn + S * logk)
  // nlogn comes from sorting the patterns.
  // log(k) comes from our usage of std::map to store edges.
  SubstringSetMatcher(const std::vector<StringPattern>& patterns);
  SubstringSetMatcher(std::vector<const StringPattern*> patterns);
  ~SubstringSetMatcher();

  // Matches |text| against all registered StringPatterns. Stores the IDs
  // of matching patterns in |matches|. |matches| is not cleared before adding
  // to it.
  // Complexity:
  //    Let t = length of |text|.
  //    Let k = range of char. Generally 256.
  //    Let z = number of matches returned.
  // Complexity = O(t * logk + zlogz)
  bool Match(const std::string& text,
             std::set<StringPattern::ID>* matches) const;

  // Returns true if this object retains no allocated data.
  bool IsEmpty() const { return is_empty_; }

  // Returns the dynamically allocated memory usage in bytes. See
  // base/trace_event/memory_usage_estimator.h for details.
  size_t EstimateMemoryUsage() const;

 private:
  // A node of an Aho Corasick Tree. See
  // http://web.stanford.edu/class/archive/cs/cs166/cs166.1166/lectures/02/Small02.pdf
  // to understand the algorithm.
  //
  // The algorithm is based on the idea of building a trie of all registered
  // patterns. Each node of the tree is annotated with a set of pattern
  // IDs that are used to report matches.
  //
  // The root of the trie represents an empty match. If we were looking whether
  // any registered pattern matches a text at the beginning of the text (i.e.
  // whether any pattern is a prefix of the text), we could just follow
  // nodes in the trie according to the matching characters in the text.
  // E.g., if text == "foobar", we would follow the trie from the root node
  // to its child labeled 'f', from there to child 'o', etc. In this process we
  // would report all pattern IDs associated with the trie nodes as matches.
  //
  // As we are not looking for all prefix matches but all substring matches,
  // this algorithm would need to compare text.substr(0), text.substr(1), ...
  // against the trie, which is in O(|text|^2).
  //
  // The Aho Corasick algorithm improves this runtime by using failure edges.
  // In case we have found a partial match of length k in the text
  // (text[i, ..., i + k - 1]) in the trie starting at the root and ending at
  // a node at depth k, but cannot find a match in the trie for character
  // text[i + k] at depth k + 1, we follow a failure edge. This edge
  // corresponds to the longest proper suffix of text[i, ..., i + k - 1] that
  // is a prefix of any registered pattern.
  //
  // If your brain thinks "Forget it, let's go shopping.", don't worry.
  // Take a nap and read an introductory text on the Aho Corasick algorithm.
  // It will make sense. Eventually.
  class AhoCorasickNode {
   public:
    using Edges = base::flat_map<char, AhoCorasickNode*>;

    AhoCorasickNode();
    ~AhoCorasickNode();
    AhoCorasickNode(AhoCorasickNode&& other);
    AhoCorasickNode& operator=(AhoCorasickNode&& other);

    AhoCorasickNode* GetEdge(char c) const;
    void SetEdge(char c, AhoCorasickNode* node);
    const Edges& edges() const { return edges_; }

    void ShrinkEdges() { edges_.shrink_to_fit(); }

    const AhoCorasickNode* failure() const { return failure_; }
    void SetFailure(const AhoCorasickNode* failure);

    void SetMatchID(StringPattern::ID id) {
      DCHECK(!IsEndOfPattern());
      match_id_ = id;
    }

    // Returns true if this node corresponds to a pattern.
    bool IsEndOfPattern() const {
      return match_id_ != StringPattern::kInvalidId;
    }

    // Must only be called if |IsEndOfPattern| returns true for this node.
    StringPattern::ID GetMatchID() const {
      DCHECK(IsEndOfPattern());
      return match_id_;
    }

    void SetOutputLink(const AhoCorasickNode* node);
    const AhoCorasickNode* output_link() const { return output_link_; }

    // Adds all pattern IDs to |matches| which are a suffix of the string
    // represented by this node.
    void AccumulateMatches(std::set<StringPattern::ID>* matches) const;

    size_t EstimateMemoryUsage() const;

   private:
    // Outgoing edges of current node.
    Edges edges_;

    // Node that failure edge leads to. The failure node corresponds to the node
    // which represents the longest proper suffix (include empty string) of the
    // string represented by this node. Must be valid, null when uninitialized.
    const AhoCorasickNode* failure_ = nullptr;

    // If valid, this node represents the end of a pattern. It stores the ID of
    // the corresponding pattern.
    StringPattern::ID match_id_ = StringPattern::kInvalidId;

    // Node that corresponds to the longest proper suffix (including empty
    // suffix) of this node and which also represents the end of a pattern. Can
    // be null.
    const AhoCorasickNode* output_link_ = nullptr;
  };

  using SubstringPatternVector = std::vector<const StringPattern*>;

  void BuildAhoCorasickTree(const SubstringPatternVector& patterns);

  // Inserts a path for |pattern->pattern()| into the tree and adds
  // |pattern->id()| to the set of matches.
  void InsertPatternIntoAhoCorasickTree(const StringPattern* pattern);

  void CreateFailureAndOutputEdges();

  // The nodes of a Aho-Corasick tree.
  std::vector<AhoCorasickNode> tree_;

  bool is_empty_ = true;

  DISALLOW_COPY_AND_ASSIGN(SubstringSetMatcher);
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_
