// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_AMBIENT_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_AMBIENT_CLIENT_IMPL_H_

#include <memory>

#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/memory/weak_ptr.h"

class GoogleServiceAuthError;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

// Class to provide profile related info.
class AmbientClientImpl : public ash::AmbientClient {
 public:
  AmbientClientImpl();
  ~AmbientClientImpl() override;

  // ash::AmbientClient:
  bool IsAmbientModeAllowedForActiveUser() override;
  void RequestAccessToken(GetAccessTokenCallback callback) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  void GetAccessToken(GetAccessTokenCallback callback,
                      const std::string& gaia_id,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  base::WeakPtrFactory<AmbientClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_AMBIENT_CLIENT_IMPL_H_
