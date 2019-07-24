// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/journey/journey_info_fetcher.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/journey/journey_info_json_request.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace journey {

namespace {
const char kChromeSyncScope[] = "https://www.googleapis.com/auth/chromememex";
}  // namespace

JourneyInfoFetcher::JourneyInfoFetcher(
    signin::IdentityManager* identity_manager,
    const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory)
    : identity_manager_(identity_manager), loader_factory_(loader_factory) {}

JourneyInfoFetcher::~JourneyInfoFetcher() = default;

void JourneyInfoFetcher::FetchJourneyInfo(
    std::vector<int64_t> timestamps,
    FetchResponseAvailableCallback callback) {
  if (!identity_manager_->HasPrimaryAccount()) {
    FetchFinished(std::move(callback), /*result=*/base::nullopt,
                  "Primary Account is not Available. Sign in is required");
    return;
  }
  pending_requests_.emplace(std::move(timestamps), std::move(callback));
  RequestOAuthTokenService();
}

void JourneyInfoFetcher::RequestOAuthTokenService() {
  if (token_fetcher_)
    return;

  OAuth2AccessTokenManager::ScopeSet scopes{kChromeSyncScope};
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "journey_info", identity_manager_, scopes,
      base::BindOnce(&JourneyInfoFetcher::AccessTokenFetchFinished,
                     base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void JourneyInfoFetcher::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      token_fetcher_deleter(std::move(token_fetcher_));

  if (error.state() != GoogleServiceAuthError::NONE) {
    AccessTokenError(error);
    return;
  }

  DCHECK(!access_token_info.token.empty());

  while (!pending_requests_.empty()) {
    std::pair<std::vector<int64_t>, FetchResponseAvailableCallback>
        timestamp_and_callback = std::move(pending_requests_.front());
    pending_requests_.pop();
    StartRequest(timestamp_and_callback.first,
                 std::move(timestamp_and_callback.second),
                 access_token_info.token);
  }
}

void JourneyInfoFetcher::AccessTokenError(const GoogleServiceAuthError& error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  DLOG(WARNING) << "JourneyInfoFetcher::AccessTokenError "
                   "Unable to get token: "
                << error.ToString();

  while (!pending_requests_.empty()) {
    std::pair<std::vector<int64_t>, FetchResponseAvailableCallback>
        timestamp_and_callback = std::move(pending_requests_.front());
    pending_requests_.pop();

    FetchFinished(std::move(timestamp_and_callback.second),
                  /*result=*/base::nullopt, error.ToString());
  }
}

void JourneyInfoFetcher::StartRequest(const std::vector<int64_t>& timestamps,
                                      FetchResponseAvailableCallback callback,
                                      const std::string& oauth_access_token) {
  JourneyInfoJsonRequest::Builder builder;
  builder.SetTimestamps(timestamps)
      .SetAuthentication(
          base::StringPrintf("bearer %s", oauth_access_token.c_str()))
      .SetParseJsonCallback(base::BindRepeating(
          &data_decoder::SafeJsonParser::Parse,
          /*connector=*/nullptr));  // This is an Android-only component,
                                    // connector is unused on Android.
  auto json_request = builder.Build();
  JourneyInfoJsonRequest* raw_request = json_request.get();
  raw_request->Start(
      base::BindOnce(&JourneyInfoFetcher::JsonRequestDone,
                     base::Unretained(this), std::move(json_request),
                     std::move(callback)),
      loader_factory_);
}

const std::string& JourneyInfoFetcher::GetLastJsonForDebugging() const {
  return last_fetch_json_;
}

void JourneyInfoFetcher::JsonRequestDone(
    std::unique_ptr<JourneyInfoJsonRequest> request,
    FetchResponseAvailableCallback callback,
    base::Optional<base::Value> result,
    const std::string& error_detail) {
  DCHECK(request);

  last_fetch_json_ = request->GetResponseString();
  FetchFinished(std::move(callback), std::move(result), error_detail);
}

void JourneyInfoFetcher::FetchFinished(FetchResponseAvailableCallback callback,
                                       base::Optional<base::Value> result,
                                       const std::string& error_detail) {
  DCHECK((result && !result->is_none()) || error_detail != "");

  // Returned |result| can be empty while |error_detail| is "".
  std::move(callback).Run(std::move(result), error_detail);
}

}  // namespace journey
