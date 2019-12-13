// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_REALTIME_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_REALTIME_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

using RTLookupRequestCallback =
    base::OnceCallback<void(std::unique_ptr<RTLookupRequest>)>;

using RTLookupResponseCallback =
    base::OnceCallback<void(std::unique_ptr<RTLookupResponse>)>;

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
class RealTimeUrlLookupService {
 public:
  explicit RealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory>);
  ~RealTimeUrlLookupService();

  // Returns true if |url|'s scheme can be checked.
  bool CanCheckUrl(const GURL& url) const;

  // Returns true if the real time lookups are currently in backoff mode due to
  // too many prior errors. If this happens, the checking falls back to
  // local hash-based method.
  bool IsInBackoffMode() const;

  // Start the full URL lookup for |url|, call |request_callback| on the same
  // thread when request is sent, call |response_callback| on the same thread
  // when response is received.
  void StartLookup(const GURL& url,
                   RTLookupRequestCallback request_callback,
                   RTLookupResponseCallback response_callback);

  // Returns the SBThreatType for a given
  // RTLookupResponse::ThreatInfo::ThreatType
  static SBThreatType GetSBThreatTypeForRTThreatType(
      RTLookupResponse::ThreatInfo::ThreatType rt_threat_type);

 private:
  using PendingRTLookupRequests =
      base::flat_map<network::SimpleURLLoader*, RTLookupResponseCallback>;

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
  // |response_body| is the response received.
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           base::TimeTicks request_start_time,
                           std::unique_ptr<std::string> response_body);

  std::unique_ptr<RTLookupRequest> FillRequestProto(const GURL& url);

  // Helper function to return a weak pointer.
  base::WeakPtr<RealTimeUrlLookupService> GetWeakPtr();

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

  friend class RealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RealTimeUrlLookupService);

};  // class RealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_REALTIME_URL_LOOKUP_SERVICE_H_
