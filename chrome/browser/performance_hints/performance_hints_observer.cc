// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::URLPatternWithWildcards;
using optimization_guide::proto::PerformanceClass;
using optimization_guide::proto::PerformanceHint;

namespace {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "PerformanceHintsObserverHintForURLResult" in
// src/tools/metrics/histograms/enums.xml.
enum class HintForURLResult {
  kHintNotFound = 0,
  kHintNotReady = 1,
  kInvalidURL = 2,
  kHintFound = 3,
  kMaxValue = kHintFound,
};

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with:
//  - "PerformanceHintsPerformanceClass" in
//    src/tools/metrics/histograms/enums.xml
//  - "PerformanceClass" in
//    src/components/optimization_guide/proto/performance_hints_metadata.proto
enum class UmaPerformanceClass {
  kUnknown = 0,
  kSlow = 1,
  kFast = 2,
  kMaxValue = kFast,
};

UmaPerformanceClass ToUmaPerformanceClass(PerformanceClass performance_class) {
  if (static_cast<int>(performance_class) < 0) {
    NOTREACHED();
    return UmaPerformanceClass::kUnknown;
  } else if (static_cast<int>(performance_class) >
             static_cast<int>(UmaPerformanceClass::kMaxValue)) {
    NOTREACHED();
    return UmaPerformanceClass::kUnknown;
  } else {
    return static_cast<UmaPerformanceClass>(performance_class);
  }
}

}  // namespace

const base::Feature kPerformanceHintsObserver{
    "PerformanceHintsObserver", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPerformanceHintsTreatUnknownAsFast{
    "PerformanceHintsTreatUnknownAsFast", base::FEATURE_DISABLED_BY_DEFAULT};

PerformanceHintsObserver::PerformanceHintsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypesAndTargets(
        {optimization_guide::proto::PERFORMANCE_HINTS}, {});
  }
}

PerformanceHintsObserver::~PerformanceHintsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

PerformanceClass PerformanceHintsObserver::PerformanceClassForURL(
    content::WebContents* web_contents,
    const GURL& url) {
  PerformanceClass performance_class = [&]() {
    if (!url.is_valid() || web_contents == nullptr) {
      return PerformanceClass::PERFORMANCE_UNKNOWN;
    }

    PerformanceHintsObserver* performance_hints_observer =
        PerformanceHintsObserver::FromWebContents(web_contents);
    if (performance_hints_observer == nullptr) {
      return PerformanceClass::PERFORMANCE_UNKNOWN;
    }

    base::Optional<optimization_guide::proto::PerformanceHint> hint =
        performance_hints_observer->HintForURL(url);
    return hint ? hint->performance_class()
                : PerformanceClass::PERFORMANCE_UNKNOWN;
  }();

  // Log to UMA before the override logic so we can determine how often the
  // override is happening.
  base::UmaHistogramEnumeration(
      "PerformanceHints.Observer.PerformanceClassForURL",
      ToUmaPerformanceClass(performance_class));

  if (base::FeatureList::IsEnabled(kPerformanceHintsTreatUnknownAsFast) &&
      performance_class == PerformanceClass::PERFORMANCE_UNKNOWN) {
    return PerformanceClass::PERFORMANCE_FAST;
  }

  return performance_class;
}

base::Optional<PerformanceHint> PerformanceHintsObserver::HintForURL(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Optional<PerformanceHint> hint;
  HintForURLResult hint_result = HintForURLResult::kHintNotFound;

  if (!hint_processed_) {
    hint_result = HintForURLResult::kHintNotReady;
  } else if (!url.is_valid()) {
    hint_result = HintForURLResult::kInvalidURL;
  } else {
    for (const auto& pattern_hint : hints_) {
      if (pattern_hint.first.Matches(url.spec())) {
        hint_result = HintForURLResult::kHintFound;
        hint = pattern_hint.second;
        break;
      }
    }
  }

  base::UmaHistogramEnumeration("PerformanceHints.Observer.HintForURLResult",
                                hint_result);

  return hint;
}

void PerformanceHintsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    // Use the same hints if the main frame hasn't changed.
    return;
  }

  // We've navigated to a new page, so clear out any hints from the previous
  // page.
  hints_.clear();

  if (!optimization_guide_decider_) {
    return;
  }
  if (navigation_handle->IsErrorPage()) {
    // Don't provide hints on Chrome error pages.
    return;
  }

  optimization_guide_decider_->CanApplyOptimizationAsync(
      navigation_handle, optimization_guide::proto::PERFORMANCE_HINTS,
      base::BindOnce(&PerformanceHintsObserver::ProcessPerformanceHint,
                     weak_factory_.GetWeakPtr()));
}

void PerformanceHintsObserver::ProcessPerformanceHint(
    OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& optimization_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hint_processed_ = true;

  if (decision != OptimizationGuideDecision::kTrue) {
    // Apply results are counted under
    // OptimizationGuide.ApplyDecision.PerformanceHints.
    return;
  }

  if (!optimization_metadata.performance_hints_metadata())
    return;

  const optimization_guide::proto::PerformanceHintsMetadata
      performance_hints_metadata =
          optimization_metadata.performance_hints_metadata().value();
  for (const PerformanceHint& hint :
       performance_hints_metadata.performance_hints()) {
    hints_.emplace_back(URLPatternWithWildcards(hint.wildcard_pattern()), hint);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceHintsObserver)
