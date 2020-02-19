// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CLIENT_H_

#include <memory>

#include "ash/public/cpp/ambient/photo_controller.h"
#include "base/callback.h"
#include "base/optional.h"

// The interface of a client to retrieve photos.
class PhotoClient {
 public:
  using OnTopicInfoFetchedCallback = base::OnceCallback<void(
      bool success,
      const base::Optional<ash::PhotoController::Topic>& topic)>;

  // Creates PhotoClient based on the build flag ENABLE_CROS_LIBASSISTANT.
  static std::unique_ptr<PhotoClient> Create();

  PhotoClient() = default;
  PhotoClient(const PhotoClient&) = delete;
  PhotoClient& operator=(const PhotoClient&) = delete;
  virtual ~PhotoClient() = default;

  virtual void FetchTopicInfo(OnTopicInfoFetchedCallback callback);
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CLIENT_H_
