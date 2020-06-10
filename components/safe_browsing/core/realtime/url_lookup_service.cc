// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/url_lookup_service.h"

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

const char kRealTimeLookupUrlPrefix[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/realtime";

const size_t kURLLookupTimeoutDurationInSeconds = 10;  // 10 seconds.

constexpr char kAuthHeaderBearer[] = "Bearer ";

}  // namespace

RealTimeUrlLookupService::RealTimeUrlLookupService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    VerdictCacheManager* cache_manager,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    const ChromeUserPopulation::ProfileManagementStatus&
        profile_management_status,
    bool is_under_advanced_protection,
    bool is_off_the_record,
    variations::VariationsService* variations_service)
    : RealTimeUrlLookupServiceBase(cache_manager),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      pref_service_(pref_service),
      profile_management_status_(profile_management_status),
      is_under_advanced_protection_(is_under_advanced_protection),
      is_off_the_record_(is_off_the_record),
      variations_(variations_service) {
  token_fetcher_ =
      std::make_unique<SafeBrowsingTokenFetcher>(identity_manager_);
}

void RealTimeUrlLookupService::StartLookup(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  DCHECK(url.is_valid());

  // Check cache.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  if (cache_response) {
    base::PostTask(FROM_HERE, CreateTaskTraits(ThreadID::IO),
                   base::BindOnce(std::move(response_callback),
                                  /* is_rt_lookup_successful */ true,
                                  std::move(cache_response)));
    return;
  }

  if (CanPerformFullURLLookupWithToken()) {
    token_fetcher_->Start(
        signin::ConsentLevel::kNotRequired,
        base::BindOnce(&RealTimeUrlLookupService::OnGetAccessToken,
                       weak_factory_.GetWeakPtr(), url,
                       std::move(request_callback),
                       std::move(response_callback), base::TimeTicks::Now()));
  } else {
    std::unique_ptr<RTLookupRequest> request = FillRequestProto(url);
    SendRequest(url, /* access_token_info */ base::nullopt, std::move(request),
                std::move(request_callback), std::move(response_callback));
  }
}

void RealTimeUrlLookupService::OnGetAccessToken(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    base::TimeTicks get_token_start_time,
    base::Optional<signin::AccessTokenInfo> access_token_info) {
  std::unique_ptr<RTLookupRequest> request = FillRequestProto(url);
  base::UmaHistogramTimes("SafeBrowsing.RT.GetToken.Time",
                          base::TimeTicks::Now() - get_token_start_time);
  base::UmaHistogramBoolean("SafeBrowsing.RT.HasTokenFromFetcher",
                            access_token_info.has_value());
  SendRequest(url, access_token_info, std::move(request),
              std::move(request_callback), std::move(response_callback));
}

void RealTimeUrlLookupService::SendRequest(
    const GURL& url,
    base::Optional<signin::AccessTokenInfo> access_token_info,
    std::unique_ptr<RTLookupRequest> request,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.Request.UserPopulation",
                            request->population().user_population(),
                            ChromeUserPopulation::UserPopulation_MAX + 1);
  std::string req_data;
  request->SerializeToString(&req_data);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_realtime_url_lookup",
                                          R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing can't detect that a URL is safe based on its "
            "local database, it sends the top-level URL to Google to verify it "
            "before showing a warning to the user."
          trigger:
            "When a main frame URL fails to match the local hash-prefix "
            "database of known safe URLs and a valid result from a prior "
            "lookup is not already cached, this will be sent."
          data: "The main frame URL that did not match the local safelist."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing real time URL checks by "
            "unchecking 'Protect you and your device from dangerous sites' in "
            "Chromium settings under Privacy, or by unchecking 'Make searches "
            "and browsing better (Sends URLs of pages you visit to Google)' in "
            "Chromium settings under Privacy."
          chrome_policy {
            UrlKeyedAnonymizedDataCollectionEnabled {
              policy_options {mode: MANDATORY}
              UrlKeyedAnonymizedDataCollectionEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kRealTimeLookupUrlPrefix);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";
  if (access_token_info.has_value()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({kAuthHeaderBearer, access_token_info.value().token}));
  }
  base::UmaHistogramBoolean("SafeBrowsing.RT.HasTokenInRequest",
                            access_token_info.has_value());

  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* loader = owned_loader.get();
  owned_loader->AttachStringForUpload(req_data, "application/octet-stream");
  owned_loader->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kURLLookupTimeoutDurationInSeconds));
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RealTimeUrlLookupService::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), url, loader,
                     base::TimeTicks::Now()));

  pending_requests_[owned_loader.release()] = std::move(response_callback);

  base::PostTask(FROM_HERE, CreateTaskTraits(ThreadID::IO),
                 base::BindOnce(std::move(request_callback), std::move(request),
                                access_token_info.has_value()
                                    ? access_token_info.value().token
                                    : ""));
}

void RealTimeUrlLookupService::Shutdown() {
  for (auto& pending : pending_requests_) {
    // Treat all pending requests as safe.
    auto response = std::make_unique<RTLookupResponse>();
    std::move(pending.second)
        .Run(/* is_rt_lookup_successful */ true, std::move(response));
    delete pending.first;
  }
  pending_requests_.clear();
}

RealTimeUrlLookupService::~RealTimeUrlLookupService() {}

void RealTimeUrlLookupService::OnURLLoaderComplete(
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

std::unique_ptr<RTLookupRequest> RealTimeUrlLookupService::FillRequestProto(
    const GURL& url) {
  auto request = std::make_unique<RTLookupRequest>();
  request->set_url(SanitizeURL(url).spec());
  request->set_lookup_type(RTLookupRequest::NAVIGATION);

  ChromeUserPopulation* user_population = request->mutable_population();
  user_population->set_user_population(
      IsEnhancedProtectionEnabled(*pref_service_)
          ? ChromeUserPopulation::ENHANCED_PROTECTION
          : IsExtendedReportingEnabled(*pref_service_)
                ? ChromeUserPopulation::EXTENDED_REPORTING
                : ChromeUserPopulation::SAFE_BROWSING);

  user_population->set_profile_management_status(profile_management_status_);
  user_population->set_is_history_sync_enabled(IsHistorySyncEnabled());
#if BUILDFLAG(FULL_SAFE_BROWSING)
  user_population->set_is_under_advanced_protection(
      is_under_advanced_protection_);
#endif
  user_population->set_is_incognito(is_off_the_record_);
  return request;
}

// TODO(bdea): Refactor this method into a util class as multiple SB classes
// have this method.
bool RealTimeUrlLookupService::IsHistorySyncEnabled() {
  return sync_service_ && sync_service_->IsSyncFeatureActive() &&
         !sync_service_->IsLocalSyncEnabled() &&
         sync_service_->GetActiveDataTypes().Has(
             syncer::HISTORY_DELETE_DIRECTIVES);
}

bool RealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return RealTimePolicyEngine::CanPerformFullURLLookup(
      pref_service_, is_off_the_record_, variations_);
}

bool RealTimeUrlLookupService::CanPerformFullURLLookupWithToken() const {
  return RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
      pref_service_, is_off_the_record_, sync_service_, identity_manager_,
      variations_);
}

bool RealTimeUrlLookupService::CanCheckSubresourceURL() const {
  return IsEnhancedProtectionEnabled(*pref_service_);
}

bool RealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  // Always return true, because consumer real time URL check only works when
  // safe browsing is enabled.
  return true;
}

}  // namespace safe_browsing
