// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LANGUAGE_DETECTION_H_
#define UI_ACCESSIBILITY_AX_LANGUAGE_DETECTION_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXNode;
class AXTree;

// This module implements language detection enabling Chrome to automatically
// detect the language for spans of text within the page without relying on any
// declared attributes.
//
// Language detection relies on four key data structures:
//   AXLanguageInfo represents the local language detection data for all text
//        within an AXNode.
//   AXLanguageInfoStats records statistics about AXLanguageInfo for all AXNodes
//        within a single AXTree, this is used to help give language detection
//        some context to reduce false positive language assignment.
//   AXLanguageSpan represents local language detection data for spans of text
//        within an AXNode, this is used by sub-node level language detection.
//   AXLanguageDetectionManager is in charge of managing all language detection
//        context for a single AXTree.
//
//
// Language detection is currently separated into two related implementation
// which are trying to address slightly different use cases, one operates at the
// node level, while the other operates at the sub-node level.
//
//
// Language detection at the node-level attempts to assign at most one language
// for each AXNode in order to support mixed-language pages. Node-level language
// detection is implemented as a two-pass process to reduce the assignment of
// spurious languages.
//
// After the first pass no languages have been assigned to AXNode(s), this is
// left to the second pass so that we can take use tree-level statistics to
// better inform the local language assigned.
//
// The first pass 'Detect' (entry point DetectLanguageForSubtree) walks the
// subtree from a given AXNode and attempts to detect the language of any text
// found. It records results in an instance of AXLanguageInfo which it stores on
// the AXNode, it also records statistics on the languages found in the
// AXLanguageInfoStats instance associated with each AXTree.
//
// The second pass 'Label' (entry point LabelLanguageForSubtree) walks the
// subtree from a given AXNode and attempts to find an appropriate language to
// associate with each AXNode based on a combination of the local detection
// results (AXLanguageInfo) and the global stats (AXLanguageInfoStats).
//
//
// Language detection at the sub-node level differs from node-level in that it
// operates at a much finer granularity of text, potentially down to individual
// characters in order to support mixed language sentences.
// We would like to detect languages that may only occur once throughout the
// entire document. Sub-node-level language detection is performed by using a
// language identifier constructed with a byte minimum of
// kShortTextIdentifierMinByteLength. This way, it can potentially detect the
// language of strings that are as short as one character in length.
//
// The entry point for sub-nod level language detection is
// GetLanguageAnnotationForStringAttribute.

// An instance of AXLanguageInfo is used to record the detected and assigned
// languages for a single AXNode, this data is entirely local to the AXNode.
struct AX_EXPORT AXLanguageInfo {
  AXLanguageInfo();
  ~AXLanguageInfo();

  // This is the final language we have assigned for this node during the
  // 'label' step, it is the result of merging:
  //  a) The detected language for this node
  //  b) The declared lang attribute on this node
  //  c) the (recursive) language of the parent (detected or declared).
  //
  // This will be the empty string if no language was assigned during label
  // phase.
  //
  // IETF BCP 47 Language code (rfc5646).
  // examples:
  //  'de'
  //  'de-DE'
  //  'en'
  //  'en-US'
  //  'es-ES'
  //
  // This should not be read directly by clients of AXNode, instead clients
  // should call AXNode::GetLanguage().
  std::string language;

  // Detected languages for this node sorted as returned by
  // FindTopNMostFreqLangs, which sorts in decreasing order of probability,
  // filtered to remove any unreliable results.
  std::vector<std::string> detected_languages;
};

// Each AXLanguageSpan contains a language, a probability, and start and end
// indices. The indices are used to specify the substring that contains the
// associated language. The string which the indices are relative to is not
// included in this structure.
// Also, the indices are relative to a Utf8 string.
// See documentation on GetLanguageAnnotationForStringAttribute for details
// on how to associate this object with a string.
struct AX_EXPORT AXLanguageSpan {
  int start_index;
  int end_index;
  std::string language;
  float probability;
};

// A single AXLanguageInfoStats instance is stored for each AXTree and
// represents the language detection statistics for every AXNode within that
// AXTree.
//
// We rely on these tree-level statistics to avoid spurious language detection
// assignments.
//
// The Label step will only assign a detected language to a node if that
// language is one of the dominant languages on the page.
class AX_EXPORT AXLanguageInfoStats {
 public:
  AXLanguageInfoStats();
  ~AXLanguageInfoStats();

  // Adjust our statistics to add provided detected languages.
  void Add(const std::vector<std::string>& languages);

  // Fetch the score for a given language.
  int GetScore(const std::string& lang) const;

  // Check if a given language is within the top results.
  bool CheckLanguageWithinTop(const std::string& lang);

 private:
  // Store a count of the occurrences of a given language.
  std::unordered_map<std::string, unsigned int> lang_counts_;

  // Cache of last calculated top language results.
  // A vector of pairs of (score, language) sorted by descending score.
  std::vector<std::pair<unsigned int, std::string>> top_results_;
  // Boolean recording that we have not mutated the statistics since last
  // calculating top results, setting this to false will cause recalculation
  // when the results are next fetched.
  bool top_results_valid_;

  void InvalidateTopResults();

  void GenerateTopResults();

  DISALLOW_COPY_AND_ASSIGN(AXLanguageInfoStats);
};

// An instance of AXLanguageDetectionManager manages all the context needed for
// language detection within a single AXTree.
class AX_EXPORT AXLanguageDetectionManager {
 public:
  AXLanguageDetectionManager();
  ~AXLanguageDetectionManager();

  // Detect language for each node in the subtree rooted at the given node.
  // This is the first pass in detection and labelling.
  // This only detects the language, it does not label it, for that see
  //  LabelLanguageForSubtree.
  void DetectLanguageForSubtree(AXNode* subtree_root);

  // Label language for each node in the subtree rooted at the given node.
  // This is the second pass in detection and labelling.
  // This will label the language, but relies on the earlier detection phase
  // having already completed.
  void LabelLanguageForSubtree(AXNode* subtree_root);

  // Detect and return languages for string attribute.
  // For example, if a node has name: "My name is Fred", then calling
  // GetLanguageAnnotationForStringAttribute(*node, ax::mojom::StringAttribute::
  // kName) would return language detection information about "My name is Fred".
  std::vector<AXLanguageSpan> GetLanguageAnnotationForStringAttribute(
      const AXNode& node,
      ax::mojom::StringAttribute attr);

 private:
  // TODO(chrishall): should this be stored by pointer or value?
  AXLanguageInfoStats lang_info_stats;

  void DetectLanguageForSubtreeInternal(AXNode* subtree_root);
  void LabelLanguageForSubtreeInternal(AXNode* subtree_root);

  // This language identifier is constructed with a default minimum byte length
  // of chrome_lang_id::NNetLanguageIdentifier::kMinNumBytesToConsider and is
  // used for detecting page-level languages.
  chrome_lang_id::NNetLanguageIdentifier language_identifier_;

  // This language identifier is constructed with a minimum byte length of
  // kShortTextIdentifierMinByteLength so it can be used for detecting languages
  // of shorter text (e.g. one character).
  chrome_lang_id::NNetLanguageIdentifier short_text_language_identifier_;

  DISALLOW_COPY_AND_ASSIGN(AXLanguageDetectionManager);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LANGUAGE_DETECTION_H_
