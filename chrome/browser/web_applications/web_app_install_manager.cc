// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WebAppInstallManager::WebAppInstallManager(Profile* profile)
    : InstallManager(profile),
      url_loader_(std::make_unique<WebAppUrlLoader>()) {
  data_retriever_factory_ = base::BindRepeating(
      []() { return std::make_unique<WebAppDataRetriever>(); });
}

WebAppInstallManager::~WebAppInstallManager() = default;

void WebAppInstallManager::Shutdown() {
  is_shutting_down_ = true;

  InstallManager::Shutdown();
  {
    TaskQueue empty;
    task_queue_.swap(empty);
    is_running_queued_task_ = false;
  }
  tasks_.clear();

  web_contents_.reset();
  web_contents_ready_ = false;
}

bool WebAppInstallManager::CanInstallWebApp(
    content::WebContents* web_contents) {
  Profile* web_contents_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  return AreWebAppsUserInstallable(web_contents_profile) &&
         IsValidWebAppUrl(web_contents->GetLastCommittedURL());
}

void WebAppInstallManager::InstallWebAppFromManifest(
    content::WebContents* contents,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppFromManifest(
      contents, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppFromManifestWithFallback(
      contents, force_shortcut_app, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool no_network_install,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppFromInfo(
      std::move(web_application_info), no_network_install, install_source,
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppWithOptions(
    content::WebContents* web_contents,
    const ExternalInstallOptions& install_options,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppWithOptions(
      web_contents, install_options,
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallOrUpdateWebAppFromSync(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  if (is_shutting_down_)
    return;

  if (finalizer()->CanSkipAppUpdateForSync(app_id, *web_application_info)) {
    std::move(callback).Run(app_id, InstallResultCode::kAlreadyInstalled);
    return;
  }

  bool is_locally_installed = registrar()->IsInstalled(app_id);
#if defined(OS_CHROMEOS)
  // On Chrome OS, sync always locally installs an app.
  is_locally_installed = true;
#endif

  CreateWebContentsIfNecessary();
  DCHECK(web_contents_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile(), finalizer(), data_retriever_factory_.Run());

  base::OnceClosure task_closure = base::BindOnce(
      &WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons,
      base::Unretained(task.get()), web_contents_.get(),
      std::move(web_application_info), is_locally_installed,
      WebappInstallSource::SYNC,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  task_queue_.push(std::move(task_closure));

  tasks_.insert(std::move(task));

  if (web_contents_ready_)
    MaybeStartQueuedTask();
}

void WebAppInstallManager::InstallWebAppForTesting(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  InstallWebAppFromInfo(std::move(web_application_info),
                        /*no_network_install=*/false,
                        /*install_source=*/WebappInstallSource::DEVTOOLS,
                        std::move(callback));
}

void WebAppInstallManager::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void WebAppInstallManager::MaybeStartQueuedTask() {
  if (is_shutting_down_)
    return;

  DCHECK(web_contents_ready_);
  DCHECK(!task_queue_.empty());

  if (is_running_queued_task_)
    return;

  is_running_queued_task_ = true;

  base::OnceClosure task_closure = std::move(task_queue_.front());
  task_queue_.pop();

  std::move(task_closure).Run();
}

void WebAppInstallManager::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

void WebAppInstallManager::OnTaskCompleted(WebAppInstallTask* task,
                                           OnceInstallCallback callback,
                                           const AppId& app_id,
                                           InstallResultCode code) {
  DCHECK(tasks_.contains(task));
  tasks_.erase(task);

  std::move(callback).Run(app_id, code);
}

void WebAppInstallManager::OnQueuedTaskCompleted(WebAppInstallTask* task,
                                                 OnceInstallCallback callback,
                                                 const AppId& app_id,
                                                 InstallResultCode code) {
  DCHECK(is_running_queued_task_);
  is_running_queued_task_ = false;

  OnTaskCompleted(task, std::move(callback), app_id, code);
  task = nullptr;

  if (task_queue_.empty()) {
    web_contents_.reset();
    web_contents_ready_ = false;
  } else {
    MaybeStartQueuedTask();
  }
}

void WebAppInstallManager::CreateWebContentsIfNecessary() {
  if (web_contents_)
    return;

  DCHECK(!web_contents_ready_);
  DCHECK(!is_shutting_down_);

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));

  // Load about:blank so that the process actually starts.
  url_loader_->LoadUrl(GURL("about:blank"), web_contents_.get(),
                       base::BindOnce(&WebAppInstallManager::OnWebContentsReady,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallManager::OnWebContentsReady(WebAppUrlLoader::Result result) {
  DCHECK_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
  web_contents_ready_ = true;

  MaybeStartQueuedTask();
}

}  // namespace web_app
