// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class NavigationHandle;
}  // namespace content

class Profile;

namespace lookalikes {

struct DomainInfo;

// Returns true if the Levenshtein distance between |str1| and |str2| is at most
// one. This has O(max(n,m)) complexity as opposed to O(n*m) of the usual edit
// distance computation.
bool IsEditDistanceAtMostOne(const base::string16& str1,
                             const base::string16& str2);

// Observes navigations and shows an interstitial if the navigated domain name
// is visually similar to a top domain or a domain with a site engagement score.
class LookalikeUrlNavigationThrottle : public content::NavigationThrottle {
 public:
  // Used for metrics. Multiple events can occur per navigation.
  enum class NavigationSuggestionEvent {
    kNone = 0,
    // Interstitial results recorded using security_interstitials::MetricsHelper
    // kInfobarShown = 1,
    // kLinkClicked = 2,
    kMatchTopSite = 3,
    kMatchSiteEngagement = 4,
    kMatchEditDistance = 5,
    kMatchEditDistanceSiteEngagement = 6,

    // Append new items to the end of the list above; do not modify or
    // replace existing values. Comment out obsolete items.
    kMaxValue = kMatchEditDistanceSiteEngagement,
  };

  static const char kHistogramName[];

  explicit LookalikeUrlNavigationThrottle(content::NavigationHandle* handle);
  ~LookalikeUrlNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

  static std::unique_ptr<LookalikeUrlNavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

 private:
  FRIEND_TEST_ALL_PREFIXES(LookalikeUrlNavigationThrottleTest,
                           IsEditDistanceAtMostOne);

  ThrottleCheckResult HandleThrottleRequest(const GURL& url);

  // Performs synchronous top domain and engaged site checks on the navigated
  // |url|. Uses |engaged_sites| for the engaged site checks.
  ThrottleCheckResult PerformChecks(
      const GURL& url,
      const DomainInfo& navigated_domain,
      const std::vector<DomainInfo>& engaged_sites);

  // A void-returning variant, only used with deferred throttle results.
  void PerformChecksDeferred(const GURL& url,
                             const DomainInfo& navigated_domain,
                             const std::vector<DomainInfo>& engaged_sites);

  bool ShouldDisplayInterstitial(
      LookalikeUrlInterstitialPage::MatchType match_type) const;

  // Returns true if a domain is visually similar to the hostname of |url|. The
  // matching domain can be a top domain or an engaged site. Similarity check
  // is made using both visual skeleton and edit distance comparison. If this
  // returns true, match details will be written into |matched_domain| and
  // |match_type|. They cannot be nullptr.
  bool GetMatchingDomain(const DomainInfo& navigated_domain,
                         const std::vector<DomainInfo>& engaged_sites,
                         std::string* matched_domain,
                         LookalikeUrlInterstitialPage::MatchType* match_type);

  ThrottleCheckResult ShowInterstitial(
      const GURL& safe_domain,
      const GURL& url,
      ukm::SourceId source_id,
      LookalikeUrlInterstitialPage::MatchType match_type);

  bool interstitials_enabled_;

  Profile* profile_;
  base::WeakPtrFactory<LookalikeUrlNavigationThrottle> weak_factory_{this};
};

}  // namespace lookalikes

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_
