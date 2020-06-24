// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_data_provider_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "media/base/media_switches.h"

namespace {

// The number of top media feeds to load for potential display.
constexpr unsigned kMediaFeedsLoadLimit = 5u;

// The minimum audio+video watchtime that is needed for display in Kaleidoscope.
constexpr base::TimeDelta kMediaFeedsAudioVideoWatchtimeMin =
    base::TimeDelta::FromMinutes(30);

// The minimum number of items a media feed needs to be displayed. This is the
// number of items needed to populate a collection.
constexpr int kMediaFeedsFetchedItemsMin = 4;

// The maximum number of feed items to display.
constexpr int kMediaFeedsItemsMaxCount = 20;

constexpr char kChromeMediaRecommendationsOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-media-recommendations";

}  // namespace

KaleidoscopeDataProviderImpl::KaleidoscopeDataProviderImpl(
    mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> receiver,
    Profile* profile)
    : credentials_(media::mojom::Credentials::New()),
      profile_(profile),
      receiver_(this, std::move(receiver)) {
  DCHECK(profile);

  // If this is Google Chrome then we should use the official API key.
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    credentials_->api_key = is_stable_channel
                                ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager_);
}

KaleidoscopeDataProviderImpl::~KaleidoscopeDataProviderImpl() = default;

void KaleidoscopeDataProviderImpl::GetCredentials(GetCredentialsCallback cb) {
  // If the user is not signed in, return the credentials without an access
  // token. Sync consent is not required to use Kaleidoscope.
  if (!identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired)) {
    std::move(cb).Run(credentials_.Clone());
    return;
  }

  pending_callbacks_.push_back(std::move(cb));

  // Get an OAuth token for the backend API. This token will be limited to just
  // our backend scope. Destroying |token_fetcher_| will cancel the fetch so
  // unretained is safe here.
  signin::ScopeSet scopes = {kChromeMediaRecommendationsOAuth2Scope};
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "kaleidoscope_service", identity_manager_, scopes,
      base::BindOnce(&KaleidoscopeDataProviderImpl::OnAccessTokenAvailable,
                     base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kNotRequired);
}

void KaleidoscopeDataProviderImpl::GetTopMediaFeeds(
    GetTopMediaFeedsCallback callback) {
  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest::
          CreateTopFeedsForDisplay(
              kMediaFeedsLoadLimit, kMediaFeedsAudioVideoWatchtimeMin,
              kMediaFeedsFetchedItemsMin,
              // Require Safe Search checking if the integration is enabled.
              base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch)),
      std::move(callback));
}

void KaleidoscopeDataProviderImpl::GetMediaFeedContents(
    int64_t feed_id,
    GetMediaFeedContentsCallback callback) {
  GetMediaHistoryService()->GetMediaFeedItems(
      media_history::MediaHistoryKeyedService::GetMediaFeedItemsRequest::
          CreateItemsForFeed(
              feed_id, kMediaFeedsItemsMaxCount,
              // Require Safe Search checking if the integration is enabled.
              base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch)),
      std::move(callback));
}

media_history::MediaHistoryKeyedService*
KaleidoscopeDataProviderImpl::GetMediaHistoryService() {
  return media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
      profile_);
}

void KaleidoscopeDataProviderImpl::OnAccessTokenAvailable(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::State::NONE)
    credentials_->access_token = access_token_info.token;

  for (auto& callback : pending_callbacks_)
    std::move(callback).Run(credentials_.Clone());

  pending_callbacks_.clear();
}
