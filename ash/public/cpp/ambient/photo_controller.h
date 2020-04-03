// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Interface for a class which is responsible for managing photos in the ambient
// mode in ash.
class ASH_PUBLIC_EXPORT PhotoController {
 public:
  // TODO(b/148462355): Add fields of weather and time info.
  // The contents shown in the Ambient mode.
  // Corresponding to backdrop::ScreenUpdate::Topic.
  struct Topic {
    Topic();
    ~Topic();
    Topic(const Topic&);
    Topic& operator=(const Topic&);

    // Image url.
    std::string url;

    // Optional for non-cropped portrait style images. The same image as in
    // |url| but it is not cropped, which is better for portrait displaying.
    base::Optional<std::string> portrait_image_url;
  };

  static PhotoController* Get();

  using PhotoDownloadCallback =
      base::OnceCallback<void(bool success, const gfx::ImageSkia&)>;
  using GetSettingsCallback =
      base::OnceCallback<void(const base::Optional<int>& topic_source)>;
  using UpdateSettingsCallback = base::OnceCallback<void(bool success)>;

  // Get next image.
  virtual void GetNextImage(PhotoDownloadCallback callback) = 0;

  // Get settings.
  virtual void GetSettings(GetSettingsCallback callback) = 0;

  // Update settings.
  virtual void UpdateSettings(int topic_source,
                              UpdateSettingsCallback callback) = 0;

 protected:
  PhotoController();
  virtual ~PhotoController();

 private:
  DISALLOW_COPY_AND_ASSIGN(PhotoController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_
