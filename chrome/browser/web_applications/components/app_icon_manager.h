// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_

#include <cstdint>
#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

// Exclusively used from the UI thread.
class AppIconManager {
 public:
  AppIconManager();
  virtual ~AppIconManager();

  // Returns false if no downloaded icon with precise |icon_size_in_px|.
  virtual bool HasIcon(const AppId& app_id, int icon_size_in_px) const = 0;
  // Returns false if no downloaded icon matching |icon_size_in_px|.
  virtual bool HasSmallestIcon(const AppId& app_id,
                               int icon_size_in_px) const = 0;

  // Reads icon's bitmap for an app. Returns empty SkBitmap in |callback| if IO
  // error.
  using ReadIconCallback = base::OnceCallback<void(const SkBitmap&)>;
  virtual void ReadIcon(const AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback) const = 0;

  // Reads all icon bitmaps for an app. Returns empty |icon_bitmaps| in
  // |callback| if IO error.
  using ReadAllIconsCallback =
      base::OnceCallback<void(std::map<SquareSizePx, SkBitmap> icon_bitmaps)>;
  virtual void ReadAllIcons(const AppId& app_id,
                            ReadAllIconsCallback callback) const = 0;

  // Reads smallest icon with size at least |icon_size_in_px|. Returns empty
  // SkBitmap in |callback| if IO error.
  virtual void ReadSmallestIcon(const AppId& app_id,
                                int icon_size_in_px,
                                ReadIconCallback callback) const = 0;

  // Reads smallest icon, compressed as PNG with size at least
  // |icon_size_in_px|. Returns empty |data| in |callback| if IO error.
  using ReadCompressedIconCallback =
      base::OnceCallback<void(std::vector<uint8_t> data)>;
  virtual void ReadSmallestCompressedIcon(
      const AppId& app_id,
      int icon_size_in_px,
      ReadCompressedIconCallback callback) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
