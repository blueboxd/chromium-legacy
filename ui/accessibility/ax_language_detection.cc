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

AXLanguageDetectionManager::AXLanguageDetectionManager(AXTree* tree)
    : short_text_language_identifier_(kShortTextIdentifierMinByteLength,
                                      kShortTextIdentifierMaxByteLength),
      tree_(tree) {}

AXLanguageDetectionManager::~AXLanguageDetectionManager() = default;

void AXLanguageDetectionManager::RegisterLanguageDetectionObserver() {
  // If the dynamic feature flag is not enabled then do nothing.
  if (!::switches::
          IsExperimentalAccessibilityLanguageDetectionDynamicEnabled()) {
    return;
  }

  // Construct our new Observer as requested.
  // If there is already an Observer on this Manager then this will destroy it.
  language_detection_observer_.reset(new AXLanguageDetectionObserver(tree_));
}

// Detect languages for each node.
void AXLanguageDetectionManager::DetectLanguages() {
  TRACE_EVENT0("accessibility", "AXLanguageInfo::DetectLanguages");
  if (!::switches::IsExperimentalAccessibilityLanguageDetectionEnabled()) {
    return;
  }

  DetectLanguagesForSubtree(tree_->root());
}

// Detect languages for a subtree rooted at the given subtree_root.
// Will not check feature flag.
void AXLanguageDetectionManager::DetectLanguagesForSubtree(
    AXNode* subtree_root) {
  // Only perform detection for kStaticText(s).
  //
  // Do not visit the children of kStaticText(s) as they don't have
  // interesting children for language detection.
  //
  // Since kInlineTextBox(es) contain text from their parent, any detection on
  // them is redundant. Instead they can inherit the detected language.
  if (subtree_root->data().role == ax::mojom::Role::kStaticText) {
    DetectLanguagesForNode(subtree_root);
  } else {
    // Otherwise, recurse into children for detection.
    for (AXNode* child : subtree_root->children()) {
      DetectLanguagesForSubtree(child);
    }
  }
}

// Detect languages for a single node.
// Will not descend into children.
// Will not check feature flag.
void AXLanguageDetectionManager::DetectLanguagesForNode(AXNode* node) {
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
      // Clear previously detected and labelled languages.
      lang_info->detected_languages.clear();
      lang_info->language.clear();
    } else {
      node->SetLanguageInfo(std::make_unique<AXLanguageInfo>());
      lang_info = node->GetLanguageInfo();
    }

    // Keep these results.
    lang_info->detected_languages = std::move(reliable_results);

    // Update statistics to take these results into account.
    lang_info_stats_.Add(lang_info->detected_languages);
  }
}

// Label languages for each node. This relies on DetectLanguages having already
// been run.
void AXLanguageDetectionManager::LabelLanguages() {
  TRACE_EVENT0("accessibility", "AXLanguageInfo::LabelLanguages");

  if (!::switches::IsExperimentalAccessibilityLanguageDetectionEnabled()) {
    return;
  }

  LabelLanguagesForSubtree(tree_->root());
}

// Label languages for each node in the subtree rooted at the given
// subtree_root. Will not check feature flag.
void AXLanguageDetectionManager::LabelLanguagesForSubtree(
    AXNode* subtree_root) {
  LabelLanguagesForNode(subtree_root);

  // Recurse into children to continue labelling.
  for (AXNode* child : subtree_root->children()) {
    LabelLanguagesForSubtree(child);
  }
}

// Label languages for a single node.
// Will not descend into children.
// Will not check feature flag.
void AXLanguageDetectionManager::LabelLanguagesForNode(AXNode* node) {
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

AXLanguageDetectionObserver::AXLanguageDetectionObserver(AXTree* tree)
    : tree_(tree) {
  // We expect the feature flag to have be checked before this Observer is
  // constructed, this should have been checked by
  // RegisterLanguageDetectionObserver.
  DCHECK(
      ::switches::IsExperimentalAccessibilityLanguageDetectionDynamicEnabled());

  tree_->AddObserver(this);
}

AXLanguageDetectionObserver::~AXLanguageDetectionObserver() {
  tree_->RemoveObserver(this);
}

void AXLanguageDetectionObserver::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  // TODO(chrishall): We likely want to re-consider updating or resetting
  // AXLanguageInfoStats over time to better support detection on long running
  // pages.

  // TODO(chrishall): To support pruning deleted node data from stats we should
  // consider implementing OnNodeWillBeDeleted. Other options available include:
  // 1) move lang info from AXNode into a map on AXTree so that we can fetch
  //    based on id in here
  // 2) AXLanguageInfo destructor could remove itself

  // TODO(chrishall): Possible optimisation: only run detect/label for certain
  // change.type(s)), at least NODE_CREATED, NODE_CHANGED, and SUBTREE_CREATED.

  DCHECK(tree->language_detection_manager);

  // Perform Detect and Label for each node changed or created.
  // We currently only consider kStaticText for detection.
  //
  // Note that language inheritance is now handled by AXNode::GetLanguage.
  //
  // Note that since Label no longer handles language inheritance, we only need
  // to call Label and Detect on the nodes that changed and don't need to
  // recurse.
  //
  // We do this in two passes because Detect updates page level statistics which
  // are later used by Label in order to make more accurate decisions.

  for (auto& change : changes) {
    if (change.node->data().role == ax::mojom::Role::kStaticText) {
      tree->language_detection_manager->DetectLanguagesForNode(change.node);
    }
  }

  for (auto& change : changes) {
    if (change.node->data().role == ax::mojom::Role::kStaticText) {
      tree->language_detection_manager->LabelLanguagesForNode(change.node);
    }
  }
}

}  // namespace ui
