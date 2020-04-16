// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/safe_browsing/core/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/web_ui/constants.h"

namespace safe_browsing {

bool SafeBrowsingUrlCheckerImpl::CanPerformFullURLLookup(const GURL& url) {
  return real_time_lookup_enabled_ &&
         RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
             resource_type_, enhanced_protection_enabled_) &&
         RealTimeUrlLookupService::CanCheckUrl(url);
}

void SafeBrowsingUrlCheckerImpl::OnRTLookupRequest(
    std::unique_ptr<RTLookupRequest> request,
    std::string oauth_token) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // The following is to log this RTLookupRequest on any open
  // chrome://safe-browsing pages.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, CreateTaskTraits(ThreadID::UI),
      base::BindOnce(&WebUIInfoSingleton::AddToRTLookupPings,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     *request, oauth_token),
      base::BindOnce(&SafeBrowsingUrlCheckerImpl::SetWebUIToken,
                     weak_factory_.GetWeakPtr()));
}

void SafeBrowsingUrlCheckerImpl::OnRTLookupResponse(
    bool is_rt_lookup_successful,
    std::unique_ptr<RTLookupResponse> response) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  bool is_expected_resource_type =
      (ResourceType::kMainFrame == resource_type_) ||
      ((ResourceType::kSubFrame == resource_type_) &&
       enhanced_protection_enabled_);
  DCHECK(is_expected_resource_type);

  const GURL& url = urls_[next_index_].url;

  if (!is_rt_lookup_successful) {
    PerformHashBasedCheck(url);
    return;
  }

  if (url_web_ui_token_ != -1) {
    // The following is to log this RTLookupResponse on any open
    // chrome://safe-browsing pages.
    base::PostTask(
        FROM_HERE, CreateTaskTraits(ThreadID::UI),
        base::BindOnce(&WebUIInfoSingleton::AddToRTLookupResponses,
                       base::Unretained(WebUIInfoSingleton::GetInstance()),
                       url_web_ui_token_, *response));
  }

  SBThreatType sb_threat_type = SB_THREAT_TYPE_SAFE;
  if (response && (response->threat_info_size() > 0) &&
      response->threat_info(0).verdict_type() ==
          RTLookupResponse::ThreatInfo::DANGEROUS) {
    // TODO(crbug.com/1033692): Only take the first threat info into account
    // because threat infos are returned in decreasing order of severity.
    // Consider extend it to support multiple threat types.
      sb_threat_type = RealTimeUrlLookupService::GetSBThreatTypeForRTThreatType(
          response->threat_info(0).threat_type());
  }
  OnUrlResult(url, sb_threat_type, ThreatMetadata());
}

}  // namespace safe_browsing
