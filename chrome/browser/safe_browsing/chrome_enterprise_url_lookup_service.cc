// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/safe_browsing/core/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace safe_browsing {

ChromeEnterpriseRealTimeUrlLookupService::
    ChromeEnterpriseRealTimeUrlLookupService(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        VerdictCacheManager* cache_manager,
        Profile* profile)
    : RealTimeUrlLookupServiceBase(url_loader_factory, cache_manager),
      profile_(profile) {}

ChromeEnterpriseRealTimeUrlLookupService::
    ~ChromeEnterpriseRealTimeUrlLookupService() = default;

void ChromeEnterpriseRealTimeUrlLookupService::StartLookup(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback) {
  // TODO(crbug.com/1085261): Implement this method.
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
      GetDMToken().is_valid(), profile_->IsOffTheRecord());
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckSubresourceURL() const {
  return false;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  return safe_browsing::IsSafeBrowsingEnabled(*profile_->GetPrefs());
}

policy::DMToken ChromeEnterpriseRealTimeUrlLookupService::GetDMToken() const {
  return ::safe_browsing::GetDMToken(profile_);
}

net::NetworkTrafficAnnotationTag
ChromeEnterpriseRealTimeUrlLookupService::GetTrafficAnnotationTag() const {
  // TODO(crbug.com/1085261): Implement this method.
  return net::NetworkTrafficAnnotationTag::NotReached();
}

GURL ChromeEnterpriseRealTimeUrlLookupService::GetRealTimeLookupUrl() const {
  // TODO(crbug.com/1085261): Implement this method.
  NOTREACHED();
  return GURL("");
}

}  // namespace safe_browsing
