// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace web_app {

class FileUtilsWrapper;
class WebAppRegistrar;

// Exclusively used from the UI thread.
class WebAppIconManager : public AppIconManager {
 public:
  WebAppIconManager(Profile* profile,
                    WebAppRegistrar& registrar,
                    std::unique_ptr<FileUtilsWrapper> utils);
  ~WebAppIconManager() override;

  // Writes all data (icons) for an app.
  using WriteDataCallback = base::OnceCallback<void(bool success)>;
  // TODO(https://crbug.com/1069308): Create a dedicated WriteShortcutIconsData
  // method here, so we can write shortcuts_icons_bitmaps separately.
  void WriteData(AppId app_id,
                 std::map<SquareSizePx, SkBitmap> icons,
                 std::vector<std::map<SquareSizePx, SkBitmap>> shortcut_icons,
                 WriteDataCallback callback);
  void DeleteData(AppId app_id, WriteDataCallback callback);

  // AppIconManager:
  bool HasIcons(
      const AppId& app_id,
      const std::vector<SquareSizePx>& icon_sizes_in_px) const override;
  bool HasSmallestIcon(const AppId& app_id,
                       SquareSizePx icon_size_in_px) const override;
  void ReadIcons(const AppId& app_id,
                 const std::vector<SquareSizePx>& icon_sizes_in_px,
                 ReadIconsCallback callback) const override;
  void ReadAllIcons(const AppId& app_id,
                    ReadIconsCallback callback) const override;
  void ReadAllShortcutIcons(const AppId& app_id,
                            ReadShortcutIconsCallback callback) const override;
  void ReadSmallestIcon(const AppId& app_id,
                        SquareSizePx icon_size_in_px,
                        ReadIconCallback callback) const override;
  void ReadSmallestCompressedIcon(
      const AppId& app_id,
      SquareSizePx icon_size_in_px,
      ReadCompressedIconCallback callback) const override;

  // If there is no icon at the downloaded sizes, we may resize what we can get.
  bool HasIconToResize(const AppId& app_id,
                       SquareSizePx desired_icon_size) const;
  // Looks for a larger icon first, a smaller icon second. (Resizing a large
  // icon smaller is preferred to resizing a small icon larger)
  void ReadIconAndResize(const AppId& app_id,
                         SquareSizePx desired_icon_size,
                         ReadIconsCallback callback) const;

 private:
  base::Optional<SquareSizePx> FindDownloadedSizeInPxMatchBigger(
      const AppId& app_id,
      SquareSizePx desired_size) const;
  base::Optional<SquareSizePx> FindDownloadedSizeInPxMatchSmaller(
      const AppId& app_id,
      SquareSizePx desired_size) const;

  const WebAppRegistrar& registrar_;
  base::FilePath web_apps_directory_;
  std::unique_ptr<FileUtilsWrapper> utils_;

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
