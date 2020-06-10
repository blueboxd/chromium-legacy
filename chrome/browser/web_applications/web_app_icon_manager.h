// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace web_app {

class FileUtilsWrapper;
class WebAppRegistrar;

// Exclusively used from the UI thread.
class WebAppIconManager : public AppIconManager, public AppRegistrarObserver {
 public:
  using FaviconReadCallback =
      base::RepeatingCallback<void(const AppId& app_id)>;

  WebAppIconManager(Profile* profile,
                    WebAppRegistrar& registrar,
                    std::unique_ptr<FileUtilsWrapper> utils);
  ~WebAppIconManager() override;

  // Writes all data (icons) for an app.
  using WriteDataCallback = base::OnceCallback<void(bool success)>;

  void WriteData(AppId app_id,
                 std::map<SquareSizePx, SkBitmap> icons,
                 WriteDataCallback callback);
  void WriteShortcutsMenuIconsData(
      AppId app_id,
      ShortcutsMenuIconsBitmaps shortcuts_menu_icons,
      WriteDataCallback callback);
  void DeleteData(AppId app_id, WriteDataCallback callback);

  // AppIconManager:
  void Start() override;
  void Shutdown() override;
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
  void ReadAllShortcutsMenuIcons(
      const AppId& app_id,
      ReadShortcutsMenuIconsCallback callback) const override;
  void ReadSmallestIcon(const AppId& app_id,
                        SquareSizePx icon_size_in_px,
                        ReadIconCallback callback) const override;
  void ReadSmallestCompressedIcon(
      const AppId& app_id,
      SquareSizePx icon_size_in_px,
      ReadCompressedIconCallback callback) const override;
  SkBitmap GetFavicon(const web_app::AppId& app_id) const override;

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  // If there is no icon at the downloaded sizes, we may resize what we can get.
  bool HasIconToResize(const AppId& app_id,
                       SquareSizePx desired_icon_size) const;
  // Looks for a larger icon first, a smaller icon second. (Resizing a large
  // icon smaller is preferred to resizing a small icon larger)
  void ReadIconAndResize(const AppId& app_id,
                         SquareSizePx desired_icon_size,
                         ReadIconsCallback callback) const;

  void SetFaviconReadCallbackForTesting(FaviconReadCallback callback);

 private:
  base::Optional<SquareSizePx> FindDownloadedSizeInPxMatchBigger(
      const AppId& app_id,
      SquareSizePx desired_size) const;
  base::Optional<SquareSizePx> FindDownloadedSizeInPxMatchSmaller(
      const AppId& app_id,
      SquareSizePx desired_size) const;

  void ReadFavicon(const AppId& app_id);
  void OnReadFavicon(const AppId& app_id, const SkBitmap&);

  WebAppRegistrar& registrar_;
  base::FilePath web_apps_directory_;
  std::unique_ptr<FileUtilsWrapper> utils_;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> registrar_observer_{this};

  // We cache a single low-resolution icon for each app.
  std::map<AppId, SkBitmap> favicon_cache_;

  FaviconReadCallback favicon_read_callback_;

  base::WeakPtrFactory<WebAppIconManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
