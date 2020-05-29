// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/url_lookup_service_base.h"

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"

namespace safe_browsing {

namespace {

const size_t kMaxFailuresToEnforceBackoff = 3;

const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

}  // namespace

RealTimeUrlLookupServiceBase::RealTimeUrlLookupServiceBase(
    VerdictCacheManager* cache_manager)
    : cache_manager_(cache_manager) {}

RealTimeUrlLookupServiceBase::~RealTimeUrlLookupServiceBase() = default;

// static
bool RealTimeUrlLookupServiceBase::CanCheckUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (net::IsLocalhost(url)) {
    // Includes: "//localhost/", "//localhost.localdomain/", "//127.0.0.1/"
    return false;
  }

  net::IPAddress ip_address;
  if (url.HostIsIPAddress() && ip_address.AssignFromIPLiteral(url.host()) &&
      !ip_address.IsPubliclyRoutable()) {
    // Includes: "//192.168.1.1/", "//172.16.2.2/", "//10.1.1.1/"
    return false;
  }

  return true;
}

// static
SBThreatType RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
    RTLookupResponse::ThreatInfo::ThreatType rt_threat_type) {
  switch (rt_threat_type) {
    case RTLookupResponse::ThreatInfo::WEB_MALWARE:
      return SB_THREAT_TYPE_URL_MALWARE;
    case RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case RTLookupResponse::ThreatInfo::UNCLEAR_BILLING:
      return SB_THREAT_TYPE_BILLING;
    case RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unexpected RTLookupResponse::ThreatType encountered";
      return SB_THREAT_TYPE_SAFE;
  }
}

// static
GURL RealTimeUrlLookupServiceBase::SanitizeURL(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}

base::WeakPtr<RealTimeUrlLookupServiceBase>
RealTimeUrlLookupServiceBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

size_t RealTimeUrlLookupServiceBase::GetBackoffDurationInSeconds() const {
  return did_successful_lookup_since_last_backoff_
             ? kMinBackOffResetDurationInSeconds
             : std::min(kMaxBackOffResetDurationInSeconds,
                        2 * next_backoff_duration_secs_);
}

void RealTimeUrlLookupServiceBase::HandleLookupError() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  consecutive_failures_++;

  // Any successful lookup clears both |consecutive_failures_| as well as
  // |did_successful_lookup_since_last_backoff_|.
  // On a failure, the following happens:
  // 1) if |consecutive_failures_| < |kMaxFailuresToEnforceBackoff|:
  //    Do nothing more.
  // 2) if already in the backoff mode:
  //    Do nothing more. This can happen if we had some outstanding real time
  //    requests in flight when we entered the backoff mode.
  // 3) if |did_successful_lookup_since_last_backoff_| is true:
  //    Enter backoff mode for |kMinBackOffResetDurationInSeconds| seconds.
  // 4) if |did_successful_lookup_since_last_backoff_| is false:
  //    This indicates that we've had |kMaxFailuresToEnforceBackoff| since
  //    exiting the last backoff with no successful lookups since so do an
  //    exponential backoff.

  if (consecutive_failures_ < kMaxFailuresToEnforceBackoff)
    return;

  if (IsInBackoffMode()) {
    return;
  }

  // Enter backoff mode, calculate duration.
  next_backoff_duration_secs_ = GetBackoffDurationInSeconds();
  backoff_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(next_backoff_duration_secs_),
      this, &RealTimeUrlLookupServiceBase::ResetFailures);
  did_successful_lookup_since_last_backoff_ = false;
}

void RealTimeUrlLookupServiceBase::HandleLookupSuccess() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  ResetFailures();

  // |did_successful_lookup_since_last_backoff_| is set to true only when we
  // complete a lookup successfully.
  did_successful_lookup_since_last_backoff_ = true;
}

bool RealTimeUrlLookupServiceBase::IsInBackoffMode() const {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  bool in_backoff = backoff_timer_.IsRunning();
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.RT.Backoff.State", in_backoff);
  return in_backoff;
}

void RealTimeUrlLookupServiceBase::ResetFailures() {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  consecutive_failures_ = 0;
  backoff_timer_.Stop();
}

std::unique_ptr<RTLookupResponse>
RealTimeUrlLookupServiceBase::GetCachedRealTimeUrlVerdict(const GURL& url) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  std::unique_ptr<RTLookupResponse::ThreatInfo> cached_threat_info =
      std::make_unique<RTLookupResponse::ThreatInfo>();

  base::UmaHistogramBoolean("SafeBrowsing.RT.HasValidCacheManager",
                            !!cache_manager_);

  base::TimeTicks get_cache_start_time = base::TimeTicks::Now();

  RTLookupResponse::ThreatInfo::VerdictType verdict_type =
      cache_manager_ ? cache_manager_->GetCachedRealTimeUrlVerdict(
                           url, cached_threat_info.get())
                     : RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED;

  base::UmaHistogramSparse("SafeBrowsing.RT.GetCacheResult", verdict_type);
  UMA_HISTOGRAM_TIMES("SafeBrowsing.RT.GetCache.Time",
                      base::TimeTicks::Now() - get_cache_start_time);

  if (verdict_type == RTLookupResponse::ThreatInfo::SAFE ||
      verdict_type == RTLookupResponse::ThreatInfo::DANGEROUS) {
    auto cache_response = std::make_unique<RTLookupResponse>();
    RTLookupResponse::ThreatInfo* new_threat_info =
        cache_response->add_threat_info();
    *new_threat_info = *cached_threat_info;
    return cache_response;
  }
  return nullptr;
}

void RealTimeUrlLookupServiceBase::MayBeCacheRealTimeUrlVerdict(
    const GURL& url,
    RTLookupResponse response) {
  if (response.threat_info_size() > 0) {
    base::PostTask(FROM_HERE, CreateTaskTraits(ThreadID::UI),
                   base::BindOnce(&VerdictCacheManager::CacheRealTimeUrlVerdict,
                                  base::Unretained(cache_manager_), url,
                                  response, base::Time::Now(),
                                  /* store_old_cache */ false));
  }
}

}  // namespace safe_browsing
