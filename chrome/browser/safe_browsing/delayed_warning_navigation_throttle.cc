// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/delayed_warning_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/navigation_handle.h"

namespace safe_browsing {

DelayedWarningNavigationThrottle::DelayedWarningNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

DelayedWarningNavigationThrottle::~DelayedWarningNavigationThrottle() = default;

std::unique_ptr<DelayedWarningNavigationThrottle>
DelayedWarningNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  // If the tab is being prerendered, stop here before it breaks metrics.
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (prerender::PrerenderContents::FromWebContents(web_contents)) {
    return nullptr;
  }

  // Otherwise, always insert the throttle for metrics recording.
  return std::make_unique<DelayedWarningNavigationThrottle>(navigation_handle);
}

const char* DelayedWarningNavigationThrottle::GetNameForLogging() {
  return "DelayedWarningNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DelayedWarningNavigationThrottle::WillProcessResponse() {
  DCHECK(base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings));
  SafeBrowsingUserInteractionObserver* observer =
      SafeBrowsingUserInteractionObserver::FromWebContents(
          navigation_handle()->GetWebContents());
  if (navigation_handle()->IsDownload() && observer) {
    // If the SafeBrowsing interstitial is delayed on the page, ignore
    // downloads. The observer will record the histogram entry for this.
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

}  // namespace safe_browsing
