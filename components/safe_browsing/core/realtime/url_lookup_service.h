// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

class PrefService;

namespace safe_browsing {

using RTLookupRequestCallback =
    base::OnceCallback<void(std::unique_ptr<RTLookupRequest>)>;

using RTLookupResponseCallback =
    base::OnceCallback<void(bool, std::unique_ptr<RTLookupResponse>)>;

class VerdictCacheManager;

class SafeBrowsingTokenFetcher;

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
// TODO(crbug.com/1050859): Add RTLookupService check flow.
class RealTimeUrlLookupService : public KeyedService {
 public:
  RealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      PrefService* pref_service,
      bool is_off_the_record);
  ~RealTimeUrlLookupService() override;

  // Returns true if |url|'s scheme can be checked.
  static bool CanCheckUrl(const GURL& url);

  // Returns true if real time URL lookup is enabled. The check is based on
  // pref settings of the associated profile, whether the profile is an off the
  // record profile and the finch flag.
  bool CanPerformFullURLLookup() const;

  // Returns true if real time URL lookup with token is enabled.
  bool CanPerformFullURLLookupWithToken() const;

  // Returns true if the real time lookups are currently in backoff mode due to
  // too many prior errors. If this happens, the checking falls back to
  // local hash-based method.
  bool IsInBackoffMode() const;

  // Returns true if this profile has opted-in to Enhanced Protection.
  bool IsUserEpOptedIn() const;

  // Start the full URL lookup for |url|, call |request_callback| on the same
  // thread when request is sent, call |response_callback| on the same thread
  // when response is received.
  // Note that |request_callback| is not called if there's a valid entry in the
  // cache for |url|.
  void StartLookup(const GURL& url,
                   RTLookupRequestCallback request_callback,
                   RTLookupResponseCallback response_callback);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // Returns the SBThreatType for a given
  // RTLookupResponse::ThreatInfo::ThreatType
  static SBThreatType GetSBThreatTypeForRTThreatType(
      RTLookupResponse::ThreatInfo::ThreatType rt_threat_type);

  // Helper function to return a weak pointer.
  base::WeakPtr<RealTimeUrlLookupService> GetWeakPtr();

 private:
  using PendingRTLookupRequests =
      base::flat_map<network::SimpleURLLoader*, RTLookupResponseCallback>;

  // Called to get cache from |cache_manager|. Returns the cached response if
  // there's a cache hit; nullptr otherwise.
  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url);

  // Called when the access token is obtained from |token_fetcher_|.
  void OnGetAccessToken(
      const GURL& url,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      base::TimeTicks get_token_start_time,
      base::Optional<signin::AccessTokenInfo> access_token_info);

  // Called to send the request to the Safe Browsing backend over the network.
  // It also attached an auth header if |access_token_info| has a value.
  void SendRequest(const GURL& url,
                   base::Optional<signin::AccessTokenInfo> access_token_info,
                   std::unique_ptr<RTLookupRequest> request,
                   RTLookupRequestCallback request_callback,
                   RTLookupResponseCallback response_callback);

  // Called to get cache from |cache_manager|. If a cache is found, return true.
  // Otherwise, return false.
  bool GetCachedRealTimeUrlVerdict(const GURL& url,
                                   RTLookupResponse* out_response);

  // Called to post a task to store the response keyed by the |url| in
  // |cache_manager|.
  void MayBeCacheRealTimeUrlVerdict(const GURL& url, RTLookupResponse response);

  // Returns the duration of the next backoff. Starts at
  // |kMinBackOffResetDurationInSeconds| and increases exponentially until it
  // reaches |kMaxBackOffResetDurationInSeconds|.
  size_t GetBackoffDurationInSeconds() const;

  // Called when the request to remote endpoint fails. May initiate or extend
  // backoff.
  void HandleLookupError();

  // Called when the request to remote endpoint succeeds. Resets error count and
  // ends backoff.
  void HandleLookupSuccess();

  // Resets the error count and ends backoff mode. Functionally same as
  // |HandleLookupSuccess| for now.
  void ResetFailures();

  // Called when the response from the real-time lookup remote endpoint is
  // received. |url_loader| is the unowned loader that was used to send the
  // request. |request_start_time| is the time when the request was sent.
  // |response_body| is the response received. |url| is used for calling
  // |MayBeCacheRealTimeUrlVerdict|.
  void OnURLLoaderComplete(const GURL& url,
                           network::SimpleURLLoader* url_loader,
                           base::TimeTicks request_start_time,
                           std::unique_ptr<std::string> response_body);

  std::unique_ptr<RTLookupRequest> FillRequestProto(const GURL& url);

  PendingRTLookupRequests pending_requests_;

  // Count of consecutive failures to complete URL lookup requests. When it
  // reaches |kMaxFailuresToEnforceBackoff|, we enter the backoff mode. It gets
  // reset when we complete a lookup successfully or when the backoff reset
  // timer fires.
  size_t consecutive_failures_ = 0;

  // If true, represents that one or more real time lookups did complete
  // successfully since the last backoff or Chrome never entered the breakoff;
  // if false and Chrome re-enters backoff period, the backoff duration is
  // increased exponentially (capped at |kMaxBackOffResetDurationInSeconds|).
  bool did_successful_lookup_since_last_backoff_ = true;

  // The current duration of backoff. Increases exponentially until it reaches
  // |kMaxBackOffResetDurationInSeconds|.
  size_t next_backoff_duration_secs_ = 0;

  // If this timer is running, backoff is in effect.
  base::OneShotTimer backoff_timer_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned object used for getting and storing real time url check cache.
  VerdictCacheManager* cache_manager_;

  // Unowned object used for getting access token when real time url check with
  // token is enabled.
  signin::IdentityManager* identity_manager_;

  // Unowned object used for checking sync status of the profile.
  syncer::SyncService* sync_service_;

  // Unowned object used for getting preference settings.
  PrefService* pref_service_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // A boolean indicates whether the profile associated with this
  // |url_lookup_service| is an off the record profile.
  bool is_off_the_record_;

  friend class RealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RealTimeUrlLookupService);

};  // class RealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_H_
