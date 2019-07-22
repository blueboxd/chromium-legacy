// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;
class WebAppDataRetriever;
class WebAppInstallTask;

class WebAppInstallManager final : public InstallManager {
 public:
  explicit WebAppInstallManager(Profile* profile);
  ~WebAppInstallManager() override;

  // InstallManager:
  void Shutdown() override;
  bool CanInstallWebApp(content::WebContents* web_contents) override;
  void InstallWebAppFromManifest(content::WebContents* contents,
                                 WebappInstallSource install_source,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback) override;
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) override;
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool no_network_install,
      WebappInstallSource install_source,
      OnceInstallCallback callback) override;
  void InstallWebAppWithOptions(content::WebContents* web_contents,
                                const ExternalInstallOptions& install_options,
                                OnceInstallCallback callback) override;
  void InstallOrUpdateWebAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;
  void InstallWebAppForTesting(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);
  bool has_web_contents_for_testing() const { return web_contents_ != nullptr; }

 private:
  void MaybeStartQueuedTask();
  void OnTaskCompleted(WebAppInstallTask* task,
                       OnceInstallCallback callback,
                       const AppId& app_id,
                       InstallResultCode code);
  void OnQueuedTaskCompleted(WebAppInstallTask* task,
                             OnceInstallCallback callback,
                             const AppId& app_id,
                             InstallResultCode code);

  void CreateWebContentsIfNecessary();
  void OnWebContentsReady(WebAppUrlLoader::Result result);

  DataRetrieverFactory data_retriever_factory_;

  std::unique_ptr<WebAppUrlLoader> url_loader_;

  // All owned tasks.
  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  // Tasks can be queued for sequential completion (to be run one at a time).
  // FIFO. This is a subset of |tasks_|.
  using TaskQueue = base::queue<base::OnceClosure>;
  TaskQueue task_queue_;
  bool is_running_queued_task_ = false;

  // A single WebContents, shared between tasks in |task_queue_|.
  std::unique_ptr<content::WebContents> web_contents_;
  bool web_contents_ready_ = false;

  bool is_shutting_down_ = false;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
