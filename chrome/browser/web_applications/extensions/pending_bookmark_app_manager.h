// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
class AppRegistrar;
class InstallFinalizer;
class WebAppUiManager;
}  // namespace web_app

namespace extensions {

// Implementation of web_app::PendingAppManager that manages the set of
// Bookmark Apps which are being installed, uninstalled, and updated.
//
// WebAppProvider creates an instance of this class and manages its
// lifetime. This class should only be used from the UI thread.
class PendingBookmarkAppManager final : public web_app::PendingAppManager {
 public:
  using WebContentsFactory =
      base::RepeatingCallback<std::unique_ptr<content::WebContents>(Profile*)>;
  using TaskFactory =
      base::RepeatingCallback<std::unique_ptr<BookmarkAppInstallationTask>(
          Profile*,
          web_app::AppRegistrar*,
          web_app::InstallFinalizer*,
          web_app::ExternalInstallOptions)>;

  explicit PendingBookmarkAppManager(Profile* profile);
  ~PendingBookmarkAppManager() override;

  // web_app::PendingAppManager
  void Shutdown() override;
  void Install(web_app::ExternalInstallOptions install_options,
               OnceInstallCallback callback) override;
  void InstallApps(
      std::vector<web_app::ExternalInstallOptions> install_options_list,
      const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> uninstall_urls,
                     const UninstallCallback& callback) override;

  void SetTaskFactoryForTesting(TaskFactory task_factory);
  void SetUrlLoaderForTesting(
      std::unique_ptr<web_app::WebAppUrlLoader> url_loader);

 private:
  struct TaskAndCallback;

  web_app::WebAppUiManager& GetUiManager();

  void MaybeStartNextInstallation();

  void StartInstallationTask(std::unique_ptr<TaskAndCallback> task);

  void CreateWebContentsIfNecessary();

  void OnUrlLoaded(web_app::WebAppUrlLoader::Result result);

  void UninstallPlaceholderIfNecessary();

  void OnPlaceholderUninstalled(bool succeeded);

  void OnInstalled(BookmarkAppInstallationTask::Result result);

  void CurrentInstallationFinished(const base::Optional<std::string>& app_id,
                                   web_app::InstallResultCode code);

  Profile* profile_;
  web_app::ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  // unique_ptr so that it can be replaced in tests.
  std::unique_ptr<web_app::WebAppUrlLoader> url_loader_;

  TaskFactory task_factory_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<TaskAndCallback> current_task_and_callback_;

  std::deque<std::unique_ptr<TaskAndCallback>> pending_tasks_and_callbacks_;

  bool shutting_down_ = false;

  base::WeakPtrFactory<PendingBookmarkAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
