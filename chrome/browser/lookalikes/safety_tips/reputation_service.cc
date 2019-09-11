// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tips_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"

namespace {

using chrome_browser_safety_tips::FlaggedPage;
using lookalikes::DomainInfo;
using lookalikes::LookalikeUrlNavigationThrottle;
using lookalikes::LookalikeUrlService;
using LookalikeMatchType = LookalikeUrlInterstitialPage::MatchType;
using safe_browsing::V4ProtocolManagerUtil;
using safety_tips::ReputationService;

const base::FeatureParam<bool> kEnableLookalikeEditDistance{
    &features::kSafetyTipUI, "editdistance", false};
const base::FeatureParam<bool> kEnableLookalikeEditDistanceSiteEngagement{
    &features::kSafetyTipUI, "editdistance_siteengagement", false};

bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites) {
  std::string matched_domain;
  LookalikeMatchType match_type;

  if (!LookalikeUrlNavigationThrottle::GetMatchingDomain(
          navigated_domain, engaged_sites, &matched_domain, &match_type)) {
    return false;
  }

  // If we're already displaying an interstitial, don't warn again.
  if (LookalikeUrlNavigationThrottle::ShouldDisplayInterstitial(
          match_type, navigated_domain)) {
    return false;
  }

  // Edit distance has higher false positives, so it gets its own feature param
  if (match_type == LookalikeMatchType::kEditDistance) {
    return kEnableLookalikeEditDistance.Get();
  }
  if (match_type == LookalikeMatchType::kEditDistanceSiteEngagement) {
    return kEnableLookalikeEditDistanceSiteEngagement.Get();
  }

  return true;
}

// This factory helps construct and find the singleton ReputationService linked
// to a Profile.
class ReputationServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ReputationService* GetForProfile(Profile* profile) {
    return static_cast<ReputationService*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create_service=*/true));
  }
  static ReputationServiceFactory* GetInstance() {
    return base::Singleton<ReputationServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ReputationServiceFactory>;

  ReputationServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "ReputationServiceFactory",
            BrowserContextDependencyManager::GetInstance()) {}

  ~ReputationServiceFactory() override {}

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new safety_tips::ReputationService(static_cast<Profile*>(profile));
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }

  DISALLOW_COPY_AND_ASSIGN(ReputationServiceFactory);
};

// Given a URL, generates all possible variant URLs to check the blocklist for.
// This is conceptually almost identical to safe_browsing::UrlToFullHashes, but
// without the hashing step.
//
// Note: Blocking "a.b/c/" does NOT block http://a.b/c without the trailing /.
void UrlToPatterns(const GURL& url, std::vector<std::string>* patterns) {
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &canon_host, &canon_path,
                                         &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    V4ProtocolManagerUtil::GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canon_path, canon_query,
                                                     &paths);

  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      DCHECK(path.length() == 0 || path[0] == '/');
      patterns->push_back(host + path);
    }
  }
}

security_state::SafetyTipStatus FlagTypeToSafetyTipStatus(
    FlaggedPage::FlagType type) {
  switch (type) {
    case FlaggedPage::FlagType::FlaggedPage_FlagType_UNKNOWN:
    case FlaggedPage::FlagType::FlaggedPage_FlagType_YOUNG_DOMAIN:
      // Reached if component includes these flags, which might happen to
      // support newer Chrome releases.
      return security_state::SafetyTipStatus::kNone;
    case FlaggedPage::FlagType::FlaggedPage_FlagType_BAD_REP:
      return security_state::SafetyTipStatus::kBadReputation;
  }
  NOTREACHED();
  return security_state::SafetyTipStatus::kNone;
}

}  // namespace

namespace safety_tips {

ReputationService::ReputationService(Profile* profile) : profile_(profile) {}

ReputationService::~ReputationService() {}

// static
ReputationService* ReputationService::Get(Profile* profile) {
  return ReputationServiceFactory::GetForProfile(profile);
}

void ReputationService::GetReputationStatus(const GURL& url,
                                            ReputationCheckCallback callback) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  // Skip top domains.
  const DomainInfo navigated_domain = lookalikes::GetDomainInfo(url);

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&ReputationService::GetReputationStatusWithEngagedSites,
                       weak_factory_.GetWeakPtr(), std::move(callback), url,
                       navigated_domain));
    // If the engaged sites need updating, there's nothing to do until callback.
    return;
  }

  GetReputationStatusWithEngagedSites(std::move(callback), url,
                                      navigated_domain,
                                      service->GetLatestEngagedSites());
}

void ReputationService::SetUserIgnore(content::WebContents* web_contents,
                                      const GURL& url) {
  RecordSafetyTipInteractionHistogram(web_contents,
                                      SafetyTipInteraction::kDismiss);
  warning_dismissed_origins_.insert(url::Origin::Create(url));
}

bool ReputationService::IsIgnored(const GURL& url) const {
  return warning_dismissed_origins_.count(url::Origin::Create(url)) > 0;
}

void ReputationService::GetReputationStatusWithEngagedSites(
    ReputationCheckCallback callback,
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites) {
  // 1. Engagement check
  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects.  This
  // check intentionally ignores the scheme.
  const auto already_engaged =
      std::find_if(engaged_sites.begin(), engaged_sites.end(),
                   [navigated_domain](const DomainInfo& engaged_domain) {
                     return (navigated_domain.domain_and_registry ==
                             engaged_domain.domain_and_registry);
                   });
  if (already_engaged != engaged_sites.end()) {
    std::move(callback).Run(security_state::SafetyTipStatus::kNone,
                            IsIgnored(url), url);
    return;
  }

  // 2. Server-side blocklist check.
  security_state::SafetyTipStatus status = GetUrlBlockType(url);
  if (status != security_state::SafetyTipStatus::kNone) {
    std::move(callback).Run(status, IsIgnored(url), url);
    return;
  }

  // 3. Protect against bad false positives by allowing top domains.
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      lookalikes::IsTopDomain(navigated_domain)) {
    std::move(callback).Run(security_state::SafetyTipStatus::kNone,
                            IsIgnored(url), url);
    return;
  }

  // 4. Lookalike heuristics.
  if (ShouldTriggerSafetyTipFromLookalike(url, navigated_domain,
                                          engaged_sites)) {
    std::move(callback).Run(security_state::SafetyTipStatus::kLookalike,
                            IsIgnored(url), url);
    return;
  }

  // TODO(crbug/984725): 5. Additional client-side heuristics
  std::move(callback).Run(security_state::SafetyTipStatus::kNone,
                          IsIgnored(url), url);
}

security_state::SafetyTipStatus GetUrlBlockType(const GURL& url) {
  std::vector<std::string> patterns;
  UrlToPatterns(url, &patterns);

  auto* proto = safety_tips::GetRemoteConfigProto();
  if (!proto) {
    return security_state::SafetyTipStatus::kNone;
  }

  auto flagged_pages = proto->flagged_page();
  for (const auto& pattern : patterns) {
    FlaggedPage search_target;
    search_target.set_pattern(pattern);

    auto lower = std::lower_bound(
        flagged_pages.begin(), flagged_pages.end(), search_target,
        [](const FlaggedPage& a, const FlaggedPage& b) -> bool {
          return a.pattern() < b.pattern();
        });

    while (lower != flagged_pages.end() && pattern == lower->pattern()) {
      // Skip over sites with unexpected flag types and keep looking for other
      // matches. This allows components to include flag types not handled by
      // this release.
      auto type = FlagTypeToSafetyTipStatus(lower->type());
      if (type != security_state::SafetyTipStatus::kNone) {
        return type;
      }
      ++lower;
    }
  }

  return security_state::SafetyTipStatus::kNone;
}

}  // namespace safety_tips
