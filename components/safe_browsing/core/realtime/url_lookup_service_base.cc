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
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

const size_t kMaxFailuresToEnforceBackoff = 3;

const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

const size_t kURLLookupTimeoutDurationInSeconds = 10;  // 10 seconds.

}  // namespace

RealTimeUrlLookupServiceBase::RealTimeUrlLookupServiceBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    VerdictCacheManager* cache_manager)
    : url_loader_factory_(url_loader_factory), cache_manager_(cache_manager) {}

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

std::unique_ptr<network::ResourceRequest>
RealTimeUrlLookupServiceBase::GetResourceRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetRealTimeLookupUrl();
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";
  return resource_request;
}

void RealTimeUrlLookupServiceBase::SendRequestInternal(
    std::unique_ptr<network::ResourceRequest> resource_request,
    const std::string& req_data,
    const GURL& url,
    RTLookupResponseCallback response_callback) {
  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       GetTrafficAnnotationTag());
  network::SimpleURLLoader* loader = owned_loader.get();
  owned_loader->AttachStringForUpload(req_data, "application/octet-stream");
  owned_loader->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kURLLookupTimeoutDurationInSeconds));
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RealTimeUrlLookupServiceBase::OnURLLoaderComplete,
                     GetWeakPtr(), url, loader, base::TimeTicks::Now()));

  pending_requests_[owned_loader.release()] = std::move(response_callback);
}

void RealTimeUrlLookupServiceBase::OnURLLoaderComplete(
    const GURL& url,
    network::SimpleURLLoader* url_loader,
    base::TimeTicks request_start_time,
    std::unique_ptr<std::string> response_body) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));

  auto it = pending_requests_.find(url_loader);
  DCHECK(it != pending_requests_.end()) << "Request not found";

  UMA_HISTOGRAM_TIMES("SafeBrowsing.RT.Network.Time",
                      base::TimeTicks::Now() - request_start_time);

  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      "SafeBrowsing.RT.Network.Result", net_error, response_code);

  auto response = std::make_unique<RTLookupResponse>();
  bool is_rt_lookup_successful = (net_error == net::OK) &&
                                 (response_code == net::HTTP_OK) &&
                                 response->ParseFromString(*response_body);
  base::UmaHistogramBoolean("SafeBrowsing.RT.IsLookupSuccessful",
                            is_rt_lookup_successful);
  is_rt_lookup_successful ? HandleLookupSuccess() : HandleLookupError();

  MayBeCacheRealTimeUrlVerdict(url, *response);

  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.RT.ThreatInfoSize",
                           response->threat_info_size());

  base::PostTask(FROM_HERE, CreateTaskTraits(ThreadID::IO),
                 base::BindOnce(std::move(it->second), is_rt_lookup_successful,
                                std::move(response)));

  delete it->first;
  pending_requests_.erase(it);
}

void RealTimeUrlLookupServiceBase::Shutdown() {
  for (auto& pending : pending_requests_) {
    // Treat all pending requests as safe.
    auto response = std::make_unique<RTLookupResponse>();
    std::move(pending.second)
        .Run(/* is_rt_lookup_successful */ true, std::move(response));
    delete pending.first;
  }
  pending_requests_.clear();
}

}  // namespace safe_browsing
