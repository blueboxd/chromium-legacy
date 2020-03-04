// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "chrome/browser/performance_hints/android/jni_headers/PerformanceHintsObserver_jni.h"
#endif  // OS_ANDROID

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::URLPatternWithWildcards;
using optimization_guide::proto::PerformanceClass;
using optimization_guide::proto::PerformanceHint;

namespace {

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
  kNormal = 3,
  kMaxValue = kNormal,
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

#if defined(OS_ANDROID)
static jint JNI_PerformanceHintsObserver_GetPerformanceClassForURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents,
    const base::android::JavaParamRef<jstring>& url) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return PerformanceHintsObserver::PerformanceClassForURLInternal(
      web_contents, GURL(base::android::ConvertJavaStringToUTF8(url)),
      /*record_metrics=*/false);
}
#endif  // OS_ANDROID

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

// static
PerformanceClass PerformanceHintsObserver::PerformanceClassForURL(
    content::WebContents* web_contents,
    const GURL& url) {
  return PerformanceClassForURLInternal(web_contents, url,
                                        /*record_metrics=*/true);
}

// static
PerformanceClass PerformanceHintsObserver::PerformanceClassForURLInternal(
    content::WebContents* web_contents,
    const GURL& url,
    bool record_metrics) {
  if (web_contents == nullptr) {
    return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile || !IsUserPermittedToFetchFromRemoteOptimizationGuide(profile)) {
    // We can't get performance hints if OptimizationGuide can't fetch them.
    return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  PerformanceHintsObserver* performance_hints_observer =
      PerformanceHintsObserver::FromWebContents(web_contents);
  if (performance_hints_observer == nullptr) {
    return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  HintForURLResult hint_result;
  base::Optional<PerformanceHint> hint;
  std::tie(hint_result, hint) = performance_hints_observer->HintForURL(url);
  if (record_metrics) {
    base::UmaHistogramEnumeration("PerformanceHints.Observer.HintForURLResult",
                                  hint_result);
  }

  PerformanceClass performance_class;
  switch (hint_result) {
    case HintForURLResult::kHintFound:
    case HintForURLResult::kHintNotFound:
    case HintForURLResult::kHintNotReady:
      performance_class = hint ? hint->performance_class()
                               : PerformanceClass::PERFORMANCE_UNKNOWN;
      break;
    case HintForURLResult::kInvalidURL:
    default:
      // Error or unknown case. Don't allow the override.
      return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  if (record_metrics) {
    // Log to UMA before the override logic so we can determine how often the
    // override is happening.
    base::UmaHistogramEnumeration(
        "PerformanceHints.Observer.PerformanceClassForURL",
        ToUmaPerformanceClass(performance_class));
  }

  if (performance_class == PerformanceClass::PERFORMANCE_UNKNOWN &&
      base::FeatureList::IsEnabled(kPerformanceHintsTreatUnknownAsFast)) {
    // If we couldn't get the hint or we didn't expect it on this page, give it
    // the benefit of the doubt.
    return PerformanceClass::PERFORMANCE_FAST;
  }

  return performance_class;
}

// static
void PerformanceHintsObserver::RecordPerformanceUMAForURL(
    content::WebContents* web_contents,
    const GURL& url) {
  PerformanceClassForURLInternal(web_contents, url,
                                 /*record_metrics=*/true);
}

std::tuple<PerformanceHintsObserver::HintForURLResult,
           base::Optional<PerformanceHint>>
PerformanceHintsObserver::HintForURL(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Optional<PerformanceHint> hint;
  HintForURLResult hint_result = HintForURLResult::kHintNotFound;

  if (!hint_processed_) {
    hint_result = HintForURLResult::kHintNotReady;
  } else if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
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

  return {hint_result, hint};
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
  hint_processed_ = false;

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
