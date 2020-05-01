// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_BACKDROP_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
#define ASH_AMBIENT_BACKDROP_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/assistant/internal/ambient/backdrop_client_config.h"

namespace ash {

class BackdropURLLoader;

// The Backdrop client implementation of AmbientBackendController.
class AmbientBackendControllerImpl : public AmbientBackendController {
 public:
  AmbientBackendControllerImpl();
  ~AmbientBackendControllerImpl() override;

  // AmbientBackendController:
  void FetchScreenUpdateInfo(
      OnScreenUpdateInfoFetchedCallback callback) override;
  void GetSettings(GetSettingsCallback callback) override;
  void UpdateSettings(AmbientModeTopicSource topic_source,
                      UpdateSettingsCallback callback) override;
  void SetPhotoRefreshInterval(base::TimeDelta interval) override;

 private:
  using BackdropClientConfig = chromeos::ambient::BackdropClientConfig;
  void RequestAccessToken(AmbientClient::GetAccessTokenCallback callback);

  void FetchScreenUpdateInfoInternal(OnScreenUpdateInfoFetchedCallback callback,
                                     const std::string& gaia_id,
                                     const std::string& access_token);

  void OnScreenUpdateInfoFetched(
      OnScreenUpdateInfoFetchedCallback callback,
      std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
      std::unique_ptr<std::string> response);

  void StartToGetSettings(GetSettingsCallback callback,
                          const std::string& gaia_id,
                          const std::string& access_token);

  void OnGetSettings(GetSettingsCallback callback,
                     std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
                     std::unique_ptr<std::string> response);

  void StartToUpdateSettings(AmbientModeTopicSource topic_source,
                             UpdateSettingsCallback callback,
                             const std::string& gaia_id,
                             const std::string& access_token);

  void OnUpdateSettings(UpdateSettingsCallback callback,
                        std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
                        std::unique_ptr<std::string> response);

  BackdropClientConfig backdrop_client_config_;

  base::WeakPtrFactory<AmbientBackendControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_BACKDROP_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
