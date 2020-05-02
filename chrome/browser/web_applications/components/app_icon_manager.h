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
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

// Exclusively used from the UI thread.
class AppIconManager {
 public:
  AppIconManager() = default;
  virtual ~AppIconManager() = default;

  // Returns false if any icon in |icon_sizes_in_px| is missing from downloaded
  // icons for a given app. |icon_sizes_in_px| must be sorted in ascending
  // order.
  virtual bool HasIcons(
      const AppId& app_id,
      const std::vector<SquareSizePx>& icon_sizes_in_px) const = 0;
  // Returns false if no downloaded icon matching |icon_size_in_px|.
  virtual bool HasSmallestIcon(const AppId& app_id,
                               SquareSizePx icon_size_in_px) const = 0;

  using ReadIconsCallback =
      base::OnceCallback<void(std::map<SquareSizePx, SkBitmap> icon_bitmaps)>;

  // Reads specified icon bitmaps for an app. Returns empty map in |callback| if
  // IO error. |icon_sizes_in_px| must be sorted in ascending order.
  virtual void ReadIcons(const AppId& app_id,
                         const std::vector<SquareSizePx>& icon_sizes_in_px,
                         ReadIconsCallback callback) const = 0;

  using ReadShortcutIconsCallback = base::OnceCallback<void(
      std::vector<std::map<SquareSizePx, SkBitmap>> shortcut_icons_bitmaps)>;

  // Reads bitmaps for all shortcut icons for an app. Returns a vector of
  // map<SquareSizePx, SkBitmap>. The index of a map in the vector is the same
  // as that of its corresponding shortcut in the manifest's shortcuts vector.
  // Returns empty vector in |callback| if we hit any error.
  virtual void ReadAllShortcutIcons(
      const AppId& app_id,
      ReadShortcutIconsCallback callback) const = 0;

  // Reads all icon bitmaps for an app. Returns empty |icon_bitmaps| in
  // |callback| if IO error.
  virtual void ReadAllIcons(const AppId& app_id,
                            ReadIconsCallback callback) const = 0;

  // Reads smallest icon with size at least |icon_size_in_px|. Returns empty
  // SkBitmap in |callback| if IO error.
  using ReadIconCallback = base::OnceCallback<void(const SkBitmap&)>;
  virtual void ReadSmallestIcon(const AppId& app_id,
                                SquareSizePx icon_size_in_px,
                                ReadIconCallback callback) const = 0;

  // Reads smallest icon, compressed as PNG with size at least
  // |icon_size_in_px|. Returns empty |data| in |callback| if IO error.
  using ReadCompressedIconCallback =
      base::OnceCallback<void(std::vector<uint8_t> data)>;
  virtual void ReadSmallestCompressedIcon(
      const AppId& app_id,
      SquareSizePx icon_size_in_px,
      ReadCompressedIconCallback callback) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
