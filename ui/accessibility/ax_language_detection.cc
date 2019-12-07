// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_language_detection.h"
#include <algorithm>
#include <functional>

#include "base/command_line.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

namespace {
// This is the maximum number of languages we assign per page, so only the top
// 3 languages on the top will be assigned to any node.
const auto kMaxDetectedLanguagesPerPage = 3;

// This is the maximum number of languages that cld3 will detect for each
// input we give it, 3 was recommended to us by the ML team as a good
// starting point.
const auto kMaxDetectedLanguagesPerSpan = 3;

const auto kShortTextIdentifierMinByteLength = 1;
// TODO(https://bugs.chromium.org/p/chromium/issues/detail?id=971360):
// Determine appropriate value for kShortTextIdentifierMaxByteLength.
const auto kShortTextIdentifierMaxByteLength = 1000;
}  // namespace

AXLanguageInfo::AXLanguageInfo() = default;
AXLanguageInfo::~AXLanguageInfo() = default;

AXLanguageInfoStats::AXLanguageInfoStats() : top_results_valid_(false) {}
AXLanguageInfoStats::~AXLanguageInfoStats() = default;

void AXLanguageInfoStats::Add(const std::vector<std::string>& languages) {
  // Assign languages with higher probability a higher score.
  // TODO(chrishall): consider more complex scoring
  size_t score = kMaxDetectedLanguagesPerSpan;
  for (const auto& lang : languages) {
    lang_counts_[lang] += score;
    --score;
  }

  InvalidateTopResults();
}

int AXLanguageInfoStats::GetScore(const std::string& lang) const {
  const auto& lang_count_it = lang_counts_.find(lang);
  if (lang_count_it == lang_counts_.end()) {
    return 0;
  }
  return lang_count_it->second;
}

void AXLanguageInfoStats::InvalidateTopResults() {
  top_results_valid_ = false;
}

// Check if a given language is within the top results.
bool AXLanguageInfoStats::CheckLanguageWithinTop(const std::string& lang) {
  if (!top_results_valid_) {
    GenerateTopResults();
  }

  for (const auto& item : top_results_) {
    if (lang == item.second) {
      return true;
    }
  }

  return false;
}

void AXLanguageInfoStats::GenerateTopResults() {
  top_results_.clear();

  for (const auto& item : lang_counts_) {
    top_results_.emplace_back(item.second, item.first);
  }

  // Since we store the pair as (score, language) the default operator> on pairs
  // does our sort appropriately.
  // Sort in descending order.
  std::sort(top_results_.begin(), top_results_.end(),
            std::greater<std::pair<unsigned int, std::string>>());

  // Resize down to remove all values greater than the N we are considering.
  top_results_.resize(kMaxDetectedLanguagesPerPage);

  top_results_valid_ = true;
}

AXLanguageDetectionManager::AXLanguageDetectionManager()
    : short_text_language_identifier_(kShortTextIdentifierMinByteLength,
                                      kShortTextIdentifierMaxByteLength) {}
AXLanguageDetectionManager::~AXLanguageDetectionManager() = default;

// Detect language for a subtree rooted at the given node.
void AXLanguageDetectionManager::DetectLanguageForSubtree(
    AXNode* subtree_root) {
  TRACE_EVENT0("accessibility", "AXLanguageInfo::DetectLanguageForSubtree");
  DCHECK(subtree_root);
  if (!::switches::IsExperimentalAccessibilityLanguageDetectionEnabled()) {
    return;
  }

  DetectLanguageForSubtreeInternal(subtree_root);
}

// Detect language for a subtree rooted at the given node
// will not check feature flag.
void AXLanguageDetectionManager::DetectLanguageForSubtreeInternal(
    AXNode* node) {
  if (node->IsText()) {
    // TODO(chrishall): implement strategy for nodes which are too small to get
    // reliable language detection results. Consider combination of
    // concatenation and bubbling up results.
    auto text = node->GetStringAttribute(ax::mojom::StringAttribute::kName);

    // FindTopNMostFreqLangs will pad the results with
    // NNetLanguageIdentifier::kUnknown in order to reach the requested number
    // of languages, this means we cannot rely on the results' length and we
    // have to filter the results.
    const auto results = language_identifier_.FindTopNMostFreqLangs(
        text, kMaxDetectedLanguagesPerSpan);

    std::vector<std::string> reliable_results;

    for (const auto res : results) {
      // The output of FindTopNMostFreqLangs is already sorted by byte count,
      // this seems good enough for now.
      // Only consider results which are 'reliable', this will also remove
      // 'unknown'.
      if (res.is_reliable) {
        reliable_results.push_back(res.language);
      }
    }

    // Only allocate a(n) LanguageInfo if we have results worth keeping.
    if (reliable_results.size()) {
      AXLanguageInfo* lang_info = node->GetLanguageInfo();
      if (lang_info) {
        lang_info->detected_languages.clear();
      } else {
        node->SetLanguageInfo(std::make_unique<AXLanguageInfo>());
        lang_info = node->GetLanguageInfo();
      }

      lang_info->detected_languages = std::move(reliable_results);
      lang_info_stats_.Add(lang_info->detected_languages);
    }
  }

  // TODO(chrishall): refactor this as textnodes only ever have inline text
  // boxes as children. This means we don't need to recurse except for
  // inheritance which can be handled elsewhere.
  for (AXNode* child : node->children()) {
    DetectLanguageForSubtreeInternal(child);
  }
}

// Label language for each node in the subtree rooted at the given node.
// This relies on DetectLanguageForSubtree having already been run.
void AXLanguageDetectionManager::LabelLanguageForSubtree(AXNode* subtree_root) {
  TRACE_EVENT0("accessibility", "AXLanguageInfo::LabelLanguageForSubtree");

  DCHECK(subtree_root);

  if (!::switches::IsExperimentalAccessibilityLanguageDetectionEnabled()) {
    return;
  }

  LabelLanguageForSubtreeInternal(subtree_root);
}

void AXLanguageDetectionManager::LabelLanguageForSubtreeInternal(AXNode* node) {
  AXLanguageInfo* lang_info = node->GetLanguageInfo();

  // lang_info is only attached by Detect when it thinks a node is interesting,
  // the presence of lang_info means that Detect expects the node to end up with
  // a language specified.
  //
  // If the lang_info->language is already set then we have no more work to do
  // for this node.
  if (lang_info && lang_info->language.empty()) {
    // We assign the highest probability language which is both:
    // 1) reliably detected for this node, and
    // 2) one of the top (kMaxDetectedLanguagesPerPage) languages on this page.
    //
    // This helps guard against false positives for nodes which have noisy
    // language detection results in isolation.
    for (const auto& lang : lang_info->detected_languages) {
      if (lang_info_stats_.CheckLanguageWithinTop(lang)) {
        lang_info->language = lang;
        break;
      }
    }

    // After attempting labelling we no longer need the detected results in
    // LanguageInfo, as they have no future use.
    if (lang_info->language.empty()) {
      // If no language was assigned then LanguageInfo as a whole can safely be
      // destroyed.
      node->ClearLanguageInfo();
    } else {
      // Otherwise, if we assigned a language then we need to keep
      // LanguageInfo.language, but we can clear the detected results.
      lang_info->detected_languages.clear();
    }
  }

  // Recurse into children to continue labelling.
  for (AXNode* child : node->children()) {
    LabelLanguageForSubtreeInternal(child);
  }
}

std::vector<AXLanguageSpan>
AXLanguageDetectionManager::GetLanguageAnnotationForStringAttribute(
    const AXNode& node,
    ax::mojom::StringAttribute attr) {
  std::vector<AXLanguageSpan> language_annotation;
  if (!node.HasStringAttribute(attr))
    return language_annotation;

  std::string attr_value = node.GetStringAttribute(attr);

  // Use author-provided language if present.
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kLanguage)) {
    // Use author-provided language if present.
    language_annotation.push_back(AXLanguageSpan{
        0 /* start_index */, attr_value.length() /* end_index */,
        node.GetStringAttribute(
            ax::mojom::StringAttribute::kLanguage) /* language */,
        1 /* probability */});
    return language_annotation;
  }
  // Calculate top 3 languages.
  // TODO(akihiroota): What's a reasonable number of languages to have
  // cld_3 find? Should vary.
  std::vector<chrome_lang_id::NNetLanguageIdentifier::Result> top_languages =
      short_text_language_identifier_.FindTopNMostFreqLangs(
          attr_value, kMaxDetectedLanguagesPerPage);
  // Create vector of AXLanguageSpans.
  for (const auto& result : top_languages) {
    std::vector<chrome_lang_id::NNetLanguageIdentifier::SpanInfo> ranges =
        result.byte_ranges;
    for (const auto& span_info : ranges) {
      language_annotation.push_back(
          AXLanguageSpan{span_info.start_index, span_info.end_index,
                         result.language, span_info.probability});
    }
  }
  // Sort Language Annotations by increasing start index. LanguageAnnotations
  // with lower start index should appear earlier in the vector.
  std::sort(
      language_annotation.begin(), language_annotation.end(),
      [](const AXLanguageSpan& left, const AXLanguageSpan& right) -> bool {
        return left.start_index <= right.start_index;
      });
  // Ensure that AXLanguageSpans do not overlap.
  for (size_t i = 0; i < language_annotation.size(); ++i) {
    if (i > 0) {
      DCHECK(language_annotation[i].start_index <=
             language_annotation[i - 1].end_index);
    }
  }
  return language_annotation;
}

}  // namespace ui
