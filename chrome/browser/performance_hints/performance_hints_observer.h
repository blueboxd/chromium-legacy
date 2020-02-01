// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_OBSERVER_H_

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace optimization_guide {
class URLPatternWithWildcards;
namespace proto {
class PerformanceHint;
}  // namespace proto
}  // namespace optimization_guide

class GURL;

// If enabled, PerformanceHintsObserver will be added as a tab helper and will
// fetch performance hints.
extern const base::Feature kPerformanceHintsObserver;

// Provides an interface to access PerformanceHints for the associated
// WebContents and links within it.
class PerformanceHintsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PerformanceHintsObserver> {
 public:
  ~PerformanceHintsObserver() override;
  PerformanceHintsObserver(const PerformanceHintsObserver&) = delete;
  PerformanceHintsObserver& operator=(const PerformanceHintsObserver&) = delete;

  // This callback populates |hints_| with performance information for links on
  // the current page and is called by |optimization_guide_decider_| when a
  // definite decision has been reached.
  void ProcessPerformanceHint(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& optimization_metadata);

  // Returns a PerformanceHint for a link to |url|, if one exists.
  base::Optional<optimization_guide::proto::PerformanceHint> HintForURL(
      const GURL& url) const;

 private:
  explicit PerformanceHintsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PerformanceHintsObserver>;
  friend class PerformanceHintsObserverTest;

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Initialized in constructor. It may be null if !IsOptimizationHintsEnabled.
  optimization_guide::OptimizationGuideDecider* optimization_guide_decider_ =
      nullptr;

  // URLs that match |first| should use the Performance hint in |second|.
  std::vector<std::pair<optimization_guide::URLPatternWithWildcards,
                        optimization_guide::proto::PerformanceHint>>
      hints_;

  // True if the ProcessPerformanceHint callback has been run.
  bool hint_processed_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PerformanceHintsObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_OBSERVER_H_
