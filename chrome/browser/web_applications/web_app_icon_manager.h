// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
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
  void WriteData(AppId app_id,
                 std::map<SquareSizePx, SkBitmap> icon_bitmaps,
                 WriteDataCallback callback);
  void DeleteData(AppId app_id, WriteDataCallback callback);

  // AppIconManager:
  bool HasIcon(const AppId& app_id, int icon_size_in_px) const override;
  bool HasSmallestIcon(const AppId& app_id, int icon_size_in_px) const override;
  void ReadIcon(const AppId& app_id,
                int icon_size_in_px,
                ReadIconCallback callback) const override;
  void ReadAllIcons(const AppId& app_id,
                    ReadAllIconsCallback callback) const override;
  void ReadSmallestIcon(const AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback) const override;
  void ReadSmallestCompressedIcon(
      const AppId& app_id,
      int icon_size_in_px,
      ReadCompressedIconCallback callback) const override;

 private:
  bool FindBestSizeInPx(const AppId& app_id,
                        int icon_size_in_px,
                        int* best_size_in_px) const;

  void ReadIconInternal(const AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback) const;

  const WebAppRegistrar& registrar_;
  base::FilePath web_apps_directory_;
  std::unique_ptr<FileUtilsWrapper> utils_;

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
