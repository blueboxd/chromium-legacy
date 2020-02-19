// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/photo_controller_impl.h"

#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"

PhotoControllerImpl::PhotoControllerImpl() : weak_factory_(this) {
  photo_client_ = PhotoClient::Create();
}

PhotoControllerImpl::~PhotoControllerImpl() = default;

void PhotoControllerImpl::GetNextImage(PhotoDownloadCallback callback) {
  photo_client_->FetchTopicInfo(
      base::BindOnce(&PhotoControllerImpl::OnNextImageInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PhotoControllerImpl::OnNextImageInfoFetched(
    PhotoDownloadCallback callback,
    bool success,
    const base::Optional<ash::PhotoController::Topic>& topic) {
  if (!success ||
      (topic->url.empty() && !topic->portrait_image_url.has_value())) {
    std::move(callback).Run(/*success=*/false, gfx::ImageSkia());
    return;
  }

  std::string image_url = topic->portrait_image_url.value_or(topic->url);
  AccountId account_id =
      chromeos::ProfileHelper::Get()
          ->GetUserByProfile(ProfileManager::GetActiveUserProfile())
          ->GetAccountId();
  ash::AssistantImageDownloader::GetInstance()->Download(
      account_id, GURL(image_url),
      base::BindOnce(std::move(callback), success));
}
