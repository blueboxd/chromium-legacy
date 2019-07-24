// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JOURNEY_JOURNEY_INFO_FETCHER_H_
#define COMPONENTS_JOURNEY_JOURNEY_INFO_FETCHER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

namespace base {
class Value;
}  // namespace base

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace journey {

class JourneyInfoJsonRequest;

using FetchResponseAvailableCallback =
    base::OnceCallback<void(base::Optional<base::Value>, const std::string&)>;

// This class is used to fetch SwitcherJourney information from the server.
class JourneyInfoFetcher {
 public:
  JourneyInfoFetcher(
      signin::IdentityManager* identity_manager,
      const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory);

  ~JourneyInfoFetcher();

  // This method fetches journey information based on |timestamps|,
  // and calls back to |callback| when complete.
  // TODO(meiliang): Add a parameter, GURL url, as the fetching url instead of
  // hard code the url.
  void FetchJourneyInfo(std::vector<int64_t> timestamps,
                        FetchResponseAvailableCallback callback);

  // This method gets last json response as a string.
  const std::string& GetLastJsonForDebugging() const;

 private:
  void RequestOAuthTokenService();

  void StartRequest(const std::vector<int64_t>& timestamps,
                    FetchResponseAvailableCallback callback,
                    const std::string& oauth_access_token);

  void AccessTokenFetchFinished(GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info);

  void AccessTokenError(const GoogleServiceAuthError& error);

  void FetchFinished(FetchResponseAvailableCallback callback,
                     base::Optional<base::Value> result,
                     const std::string& error_detail);

  void JsonRequestDone(std::unique_ptr<JourneyInfoJsonRequest> request,
                       FetchResponseAvailableCallback callback,
                       base::Optional<base::Value> result,
                       const std::string& error_detail);

  signin::IdentityManager* identity_manager_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  const scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  std::string last_fetch_json_;

  // This queues stores requests that wait for an access token.
  base::queue<std::pair<std::vector<int64_t>, FetchResponseAvailableCallback>>
      pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(JourneyInfoFetcher);
};

}  // namespace journey

#endif  // COMPONENTS_JOURNEY_JOURNEY_INFO_FETCHER_H_
