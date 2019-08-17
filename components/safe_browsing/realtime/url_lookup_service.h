// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_REALTIME_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_REALTIME_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "components/safe_browsing/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

using RTLookupResponseCallback =
    base::OnceCallback<void(std::unique_ptr<RTLookupResponse>)>;

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
class RealTimeUrlLookupService {
 public:
  explicit RealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory>);
  ~RealTimeUrlLookupService();

  void StartLookup(const GURL& url, RTLookupResponseCallback callback);

  // Returns true if the real time lookups are currently in backoff mode due to
  // too many prior errors. If this happens, the checking falls back to
  // local hash-based method.
  bool IsInBackoffMode();

 private:
  using PendingRTLookupRequests =
      base::flat_map<network::SimpleURLLoader*, RTLookupResponseCallback>;

  // Called when the request to remote endpoint fails. May initiate or extend
  // backoff.
  void HandleResponseError();

  // Called when the response from the real-time lookup remote endpoint is
  // received.
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::unique_ptr<std::string> response_body);

  PendingRTLookupRequests pending_requests_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};  // class RealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_REALTIME_URL_LOOKUP_SERVICE_H_
