// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app.h"
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
      profile(), shortcut_manager(), finalizer(),
      data_retriever_factory_.Run());
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
      profile(), shortcut_manager(), finalizer(),
      data_retriever_factory_.Run());
  task->InstallWebAppFromManifestWithFallback(
      contents, force_shortcut_app, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    ForInstallableSite for_installable_site,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), shortcut_manager(), finalizer(),
      data_retriever_factory_.Run());
  task->InstallWebAppFromInfo(
      std::move(web_application_info), for_installable_site, install_source,
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppWithParams(
    content::WebContents* web_contents,
    const InstallParams& install_params,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), shortcut_manager(), finalizer(),
      data_retriever_factory_.Run());
  task->InstallWebAppWithParams(
      web_contents, install_params, install_source,
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromSync(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  // Skip sync update if app exists.
  // All manifest fields will be set locally via update (see crbug.com/926083)
  // so we must not sync them in order to avoid a device-to-device sync war.
  if (registrar()->IsInstalled(app_id)) {
    std::move(callback).Run(app_id,
                            InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  bool is_locally_installed = registrar()->IsLocallyInstalled(app_id);
#if defined(OS_CHROMEOS)
  // On Chrome OS, sync always locally installs an app.
  is_locally_installed = true;
#endif

  auto task = std::make_unique<WebAppInstallTask>(
      profile(), shortcut_manager(), finalizer(),
      data_retriever_factory_.Run());

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons,
      base::Unretained(task.get()), EnsureWebContentsCreated(),
      std::move(web_application_info), is_locally_installed,
      WebappInstallSource::SYNC,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::UpdateWebAppFromInfo(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), shortcut_manager(), finalizer(),
      data_retriever_factory_.Run());

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::UpdateWebAppFromInfo, base::Unretained(task.get()),
      EnsureWebContentsCreated(), app_id, std::move(web_application_info),
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::Shutdown() {
  tasks_.clear();
  {
    TaskQueue empty;
    task_queue_.swap(empty);
  }
  web_contents_.reset();
}

void WebAppInstallManager::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps) {
  // TODO(crbug.com/860583): Implement sync-initiated app installs.
  NOTIMPLEMENTED();
}

void WebAppInstallManager::UninstallWebAppsAfterSync(
    std::vector<std::unique_ptr<WebApp>> web_apps) {
  // TODO(crbug.com/860583): Implement sync-initiated app uninstalls.
  NOTIMPLEMENTED();
}

void WebAppInstallManager::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void WebAppInstallManager::EnqueueTask(std::unique_ptr<WebAppInstallTask> task,
                                       base::OnceClosure start_task) {
  DCHECK(web_contents_);

  tasks_.insert(std::move(task));
  task_queue_.push(std::move(start_task));

  if (web_contents_ready_)
    MaybeStartQueuedTask();
}

void WebAppInstallManager::MaybeStartQueuedTask() {
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

content::WebContents* WebAppInstallManager::EnsureWebContentsCreated() {
  if (web_contents_)
    return web_contents_.get();

  DCHECK(!web_contents_ready_);

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));

  // Load about:blank so that the process actually starts.
  url_loader_->LoadUrl(GURL("about:blank"), web_contents_.get(),
                       base::BindOnce(&WebAppInstallManager::OnWebContentsReady,
                                      weak_ptr_factory_.GetWeakPtr()));

  return web_contents_.get();
}

void WebAppInstallManager::OnWebContentsReady(WebAppUrlLoader::Result result) {
  DCHECK_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
  web_contents_ready_ = true;

  MaybeStartQueuedTask();
}

}  // namespace web_app
