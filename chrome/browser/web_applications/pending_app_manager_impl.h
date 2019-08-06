// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_MANAGER_IMPL_H_

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
#include "chrome/browser/web_applications/pending_app_install_task.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Installs, uninstalls, and updates any External Web Apps. This class should
// only be used from the UI thread.
class PendingAppManagerImpl : public PendingAppManager {
 public:
  using WebContentsFactory =
      base::RepeatingCallback<std::unique_ptr<content::WebContents>(Profile*)>;

  explicit PendingAppManagerImpl(Profile* profile);
  ~PendingAppManagerImpl() override;

  // PendingAppManager:
  void Install(ExternalInstallOptions install_options,
               OnceInstallCallback callback) override;
  void InstallApps(std::vector<ExternalInstallOptions> install_options_list,
                   const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> uninstall_urls,
                     const UninstallCallback& callback) override;
  void Shutdown() override;

  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);

 protected:
  virtual std::unique_ptr<PendingAppInstallTask> CreateInstallationTask(
      ExternalInstallOptions install_options);

  Profile* profile() { return profile_; }

 private:
  struct TaskAndCallback;

  void MaybeStartNextInstallation();

  void StartInstallationTask(std::unique_ptr<TaskAndCallback> task);

  void CreateWebContentsIfNecessary();

  void OnUrlLoaded(WebAppUrlLoader::Result result);

  void UninstallPlaceholderIfNecessary();

  void OnPlaceholderUninstalled(bool succeeded);

  void OnInstalled(PendingAppInstallTask::Result result);

  void CurrentInstallationFinished(const base::Optional<std::string>& app_id,
                                   InstallResultCode code);

  Profile* profile_;
  ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  // unique_ptr so that it can be replaced in tests.
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<TaskAndCallback> current_task_and_callback_;

  std::deque<std::unique_ptr<TaskAndCallback>> pending_tasks_and_callbacks_;

  base::WeakPtrFactory<PendingAppManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingAppManagerImpl);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_MANAGER_IMPL_H_
