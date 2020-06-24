// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_

#include <memory>

#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media_history {
class MediaHistoryKeyedService;
}  // namespace media_history

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class Profile;

class KaleidoscopeDataProviderImpl
    : public media::mojom::KaleidoscopeDataProvider {
 public:
  KaleidoscopeDataProviderImpl(
      mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> receiver,
      Profile* profile);
  KaleidoscopeDataProviderImpl(const KaleidoscopeDataProviderImpl&) = delete;
  KaleidoscopeDataProviderImpl& operator=(const KaleidoscopeDataProviderImpl&) =
      delete;
  ~KaleidoscopeDataProviderImpl() override;

  // media::mojom::KaleidoscopeDataProvider implementation.
  void GetTopMediaFeeds(GetTopMediaFeedsCallback callback) override;
  void GetMediaFeedContents(int64_t feed_id,
                            GetMediaFeedContentsCallback callback) override;
  void GetCredentials(GetCredentialsCallback cb) override;

 private:
  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  // Called when an access token request completes (successfully or not).
  void OnAccessTokenAvailable(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);

  // Helper for fetching OAuth2 access tokens. This is non-null iff an access
  // token request is currently in progress.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // The current set of credentials.
  media::mojom::CredentialsPtr credentials_;

  // Pending credentials waiting on an access token.
  std::vector<GetCredentialsCallback> pending_callbacks_;

  signin::IdentityManager* identity_manager_;

  Profile* const profile_;

  mojo::Receiver<media::mojom::KaleidoscopeDataProvider> receiver_;
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_DATA_PROVIDER_IMPL_H_
