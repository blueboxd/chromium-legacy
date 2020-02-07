// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_
#define COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_

#include <stdint.h>

#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/url_matcher/string_pattern.h"
#include "components/url_matcher/url_matcher_export.h"

namespace url_matcher {

// Class that store a set of string patterns and can find for a string S,
// which string patterns occur in S.
class URL_MATCHER_EXPORT SubstringSetMatcher {
 public:
  // Registers all |patterns|. The same pattern cannot be registered twice and
  // each pattern needs to have a unique ID.
  SubstringSetMatcher(const std::vector<StringPattern>& patterns);
  SubstringSetMatcher(std::vector<const StringPattern*> patterns);
  ~SubstringSetMatcher();

  // Matches |text| against all registered StringPatterns. Stores the IDs
  // of matching patterns in |matches|. |matches| is not cleared before adding
  // to it.
  bool Match(const std::string& text,
             std::set<StringPattern::ID>* matches) const;

  // Returns true if this object retains no allocated data.
  bool IsEmpty() const { return is_empty_; }

  // Returns the dynamically allocated memory usage in bytes. See
  // base/trace_event/memory_usage_estimator.h for details.
  size_t EstimateMemoryUsage() const;

 private:
  // A node of an Aho Corasick Tree. This is implemented according to
  // http://www.cs.uku.fi/~kilpelai/BSA05/lectures/slides04.pdf
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
    // Key: label of the edge, value: pointer to child node.
    typedef std::map<char, AhoCorasickNode*> Edges;
    typedef std::set<StringPattern::ID> Matches;

    AhoCorasickNode();
    ~AhoCorasickNode();
    AhoCorasickNode(AhoCorasickNode&& other);
    AhoCorasickNode& operator=(AhoCorasickNode&& other);

    AhoCorasickNode* GetEdge(char c) const;
    void SetEdge(char c, AhoCorasickNode* node);
    const Edges& edges() const { return edges_; }

    const AhoCorasickNode* failure() const { return failure_; }
    void SetFailure(const AhoCorasickNode* failure);

    void AddMatch(StringPattern::ID id);
    void AddMatches(const Matches& matches);
    const Matches& matches() const { return matches_; }

    size_t EstimateMemoryUsage() const;

   private:
    // Outgoing edges of current node.
    Edges edges_;

    // Node that failure edge leads to. Null when uninitialized.
    const AhoCorasickNode* failure_ = nullptr;

    // Identifiers of matches.
    Matches matches_;
  };

  typedef std::vector<const StringPattern*> SubstringPatternVector;

  void BuildAhoCorasickTree(const SubstringPatternVector& patterns);

  // Inserts a path for |pattern->pattern()| into the tree and adds
  // |pattern->id()| to the set of matches.
  void InsertPatternIntoAhoCorasickTree(const StringPattern* pattern);
  void CreateFailureEdges();

  // The nodes of a Aho-Corasick tree.
  std::vector<AhoCorasickNode> tree_;

  bool is_empty_ = true;

  DISALLOW_COPY_AND_ASSIGN(SubstringSetMatcher);
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_
