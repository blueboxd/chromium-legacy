// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tips_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"

namespace {

using chrome_browser_safety_tips::FlaggedPage;
using lookalikes::DomainInfo;
using lookalikes::LookalikeUrlService;
using safe_browsing::V4ProtocolManagerUtil;
using safety_tips::ReputationService;

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

safety_tips::SafetyTipType FlagTypeToSafetyTipType(FlaggedPage::FlagType type) {
  switch (type) {
    case FlaggedPage::FlagType::FlaggedPage_FlagType_UNKNOWN:
    case FlaggedPage::FlagType::FlaggedPage_FlagType_YOUNG_DOMAIN:
      NOTREACHED();
      break;
    case FlaggedPage::FlagType::FlaggedPage_FlagType_BAD_REP:
      return safety_tips::SafetyTipType::kBadReputation;
  }
  NOTREACHED();
  return safety_tips::SafetyTipType::kNone;
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

void ReputationService::SetUserIgnore(const GURL& url) {
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
  if (already_engaged != engaged_sites.end())
    return;

  // 2. Server-side blocklist check.
  SafetyTipType type = GetUrlBlockType(url);
  if (type != SafetyTipType::kNone) {
    std::move(callback).Run(type, IsIgnored(url), url);
    return;
  }

  // TODO(crbug/984725): 3. Client-side heuristics or lookalike check.
}

SafetyTipType GetUrlBlockType(const GURL& url) {
  std::vector<std::string> patterns;
  UrlToPatterns(url, &patterns);

  auto* proto = safety_tips::GetRemoteConfigProto();
  if (!proto) {
    return SafetyTipType::kNone;
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

    if (lower != flagged_pages.end() && pattern == lower->pattern()) {
      return FlagTypeToSafetyTipType(lower->type());
    }
  }

  return SafetyTipType::kNone;
}

}  // namespace safety_tips
