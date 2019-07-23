// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager_observer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace {

// A single installation task. It is possible to have many concurrent
// installations per one |web_contents|.
// This class is a holder of WebAppDataRetriever or BookmarkAppHelper and limits
// their lifetime to match WebContents, InstallManager and Profile lifetimes.
class InstallTask : public content::WebContentsObserver,
                    public web_app::InstallManagerObserver {
 public:
  InstallTask(BookmarkAppInstallManager* install_manager,
              web_app::InstallManager::OnceInstallCallback callback)
      : WebContentsObserver(),
        callback_(std::move(callback)) {
    manager_observer_.Add(install_manager);
  }

  ~InstallTask() override { DCHECK(!callback_); }

  void SetWebContents(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  void SetBookmarkAppHelper(
      std::unique_ptr<BookmarkAppHelper> bookmark_app_helper) {
    DCHECK(!data_retriever_);
    bookmark_app_helper_ = std::move(bookmark_app_helper);
  }

  void SetDataRetriever(
      std::unique_ptr<web_app::WebAppDataRetriever> data_retriever) {
    DCHECK(!bookmark_app_helper_);
    data_retriever_ = std::move(data_retriever);
  }

  void ResetDataRetriever() { data_retriever_.reset(); }

  // WebContentsObserver:
  void WebContentsDestroyed() override {
    DCHECK(callback_);

    CallInstallCallback(web_app::AppId(),
                        web_app::InstallResultCode::kWebContentsDestroyed);
    // BookmarkAppHelper should not outsurvive |web_contents|.
    // This |reset| invalidates all weak references to BookmarkAppHelper and
    // cancels BookmarkAppHelper-related tasks posted to message loops.
    // This |ResetAndDeleteThisTask| call also destroys |this| InstallTask since
    // it is owned by a callback bind state.
    return ResetAndDeleteThisTask();
  }

  // InstallManagerObserver:
  void OnInstallManagerShutdown() override {
    DCHECK(callback_);

    CallInstallCallback(web_app::AppId(),
                        web_app::InstallResultCode::kInstallManagerDestroyed);
    // BookmarkAppHelper should not outsurvive profile and its keyed services.
    // This |ResetAndDeleteThisTask| call also destroys |this| InstallTask since
    // it is owned by a callback bind state.
    return ResetAndDeleteThisTask();
  }

  void CallInstallCallback(const web_app::AppId& app_id,
                           web_app::InstallResultCode code) {
    if (callback_)
      std::move(callback_).Run(app_id, code);

    Observe(nullptr);
    manager_observer_.RemoveAll();
  }

 private:
  void ResetAndDeleteThisTask() {
    // One of these |reset|s deletes |this| InstallTask since it is owned by a
    // callback bind state. We can not touch data members after it.
    if (data_retriever_)
      data_retriever_.reset();
    else
      bookmark_app_helper_.reset();
  }

  // The object invariant: InstallTask is owned either by data_retriever_ or
  // bookmark_app_helper_. Just one of these ptrs is valid at any moment.
  std::unique_ptr<web_app::WebAppDataRetriever> data_retriever_;
  std::unique_ptr<BookmarkAppHelper> bookmark_app_helper_;

  web_app::InstallManager::OnceInstallCallback callback_;
  ScopedObserver<web_app::InstallManager, InstallTask> manager_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(InstallTask);
};

void DestroyInstallTask(std::unique_ptr<InstallTask> install_task) {
  install_task.reset();
}

void OnBookmarkAppInstalled(std::unique_ptr<InstallTask> install_task,
                            const Extension* app,
                            const WebApplicationInfo& web_app_info) {
  if (app) {
    install_task->CallInstallCallback(app->id(),
                                      web_app::InstallResultCode::kSuccess);
  } else {
    install_task->CallInstallCallback(
        web_app::AppId(), web_app::InstallResultCode::kFailedUnknownReason);
  }

  // OnBookmarkAppInstalled is called synchronously by BookmarkAppHelper.
  // Post async task to destroy InstallTask with BookmarkAppHelper later.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(DestroyInstallTask, std::move(install_task)));
}

void SetBookmarkAppHelperOptions(const web_app::ExternalInstallOptions& options,
                                 BookmarkAppHelper* helper) {
  switch (options.launch_container) {
    case web_app::LaunchContainer::kDefault:
      break;
    case web_app::LaunchContainer::kTab:
      helper->set_forced_launch_type(LAUNCH_TYPE_REGULAR);
      break;
    case web_app::LaunchContainer::kWindow:
      helper->set_forced_launch_type(LAUNCH_TYPE_WINDOW);
      break;
  }

  switch (options.install_source) {
    // TODO(nigeltao/ortuno): should these two cases lead to different
    // Manifest::Location values: INTERNAL vs EXTERNAL_PREF_DOWNLOAD?
    case web_app::ExternalInstallSource::kInternalDefault:
    case web_app::ExternalInstallSource::kExternalDefault:
      helper->set_is_default_app();
      break;
    case web_app::ExternalInstallSource::kExternalPolicy:
      helper->set_is_policy_installed_app();
      break;
    case web_app::ExternalInstallSource::kSystemInstalled:
      helper->set_is_system_app();
      break;
    case web_app::ExternalInstallSource::kArc:
      NOTREACHED();
      break;
  }

  if (!options.add_to_applications_menu)
    helper->set_skip_adding_to_applications_menu();

  if (!options.add_to_desktop)
    helper->set_skip_adding_to_desktop();

  if (!options.add_to_quick_launch_bar)
    helper->set_skip_adding_to_quick_launch_bar();

  if (options.bypass_service_worker_check)
    helper->set_bypass_service_worker_check();

  if (options.require_manifest)
    helper->set_require_manifest();
}

void OnGetWebApplicationInfo(
    const BookmarkAppInstallManager* install_manager,
    std::unique_ptr<InstallTask> install_task,
    const web_app::ExternalInstallOptions& install_options,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (!web_app_info) {
    install_task->CallInstallCallback(
        web_app::AppId(),
        web_app::InstallResultCode::kGetWebApplicationInfoFailed);

    // OnGetWebApplicationInfo is called synchronously by WebAppDataRetriever
    // as a tail call. We can delete InstallTask with data_retriever_ safely.
    install_task.reset();
    return;
  }

  WebappInstallSource install_source =
      web_app::ConvertExternalInstallSourceToInstallSource(
          install_options.install_source);

  Profile* profile = Profile::FromBrowserContext(
      install_task->web_contents()->GetBrowserContext());
  DCHECK(profile);

  auto bookmark_app_helper = install_manager->bookmark_app_helper_factory().Run(
      profile, std::move(web_app_info), install_task->web_contents(),
      install_source);

  BookmarkAppHelper* helper_ptr = bookmark_app_helper.get();
  SetBookmarkAppHelperOptions(install_options, bookmark_app_helper.get());

  install_task->ResetDataRetriever();
  install_task->SetBookmarkAppHelper(std::move(bookmark_app_helper));

  helper_ptr->Create(base::BindRepeating(
      OnBookmarkAppInstalled, base::Passed(std::move(install_task))));
}

}  // namespace

BookmarkAppInstallManager::BookmarkAppInstallManager(Profile* profile)
    : InstallManager(profile) {
  bookmark_app_helper_factory_ = base::BindRepeating(
      [](Profile* profile, std::unique_ptr<WebApplicationInfo> web_app_info,
         content::WebContents* web_contents,
         WebappInstallSource install_source) {
        return std::make_unique<BookmarkAppHelper>(
            profile, std::move(web_app_info), web_contents, install_source);
      });

  data_retriever_factory_ = base::BindRepeating(
      []() { return std::make_unique<web_app::WebAppDataRetriever>(); });
}

BookmarkAppInstallManager::~BookmarkAppInstallManager() = default;

bool BookmarkAppInstallManager::CanInstallWebApp(
    content::WebContents* web_contents) {
  return extensions::TabHelper::FromWebContents(web_contents)
      ->CanCreateBookmarkApp();
}

void BookmarkAppInstallManager::InstallWebAppFromManifestWithFallback(
    content::WebContents* web_contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  // |dialog_callback| is ignored here: BookmarkAppHelper directly uses UI's
  // chrome::ShowPWAInstallDialog.
  // |install_source| is also ignored here: extensions::TabHelper specifies it.
  extensions::TabHelper::FromWebContents(web_contents)
      ->CreateHostedAppFromWebContents(
          force_shortcut_app,
          base::BindOnce(
              [](OnceInstallCallback callback, const ExtensionId& app_id,
                 bool success) {
                std::move(callback).Run(
                    app_id,
                    success ? web_app::InstallResultCode::kSuccess
                            : web_app::InstallResultCode::kFailedUnknownReason);
              },
              std::move(callback)));
}

void BookmarkAppInstallManager::InstallWebAppFromManifest(
    content::WebContents* web_contents,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  // |dialog_callback| is ignored here:
  // BookmarkAppHelper directly uses UI's chrome::ShowPWAInstallDialog.

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto bookmark_app_helper = std::make_unique<BookmarkAppHelper>(
      profile, std::make_unique<WebApplicationInfo>(), web_contents,
      install_source);

  BookmarkAppHelper* helper = bookmark_app_helper.get();

  auto install_task = std::make_unique<InstallTask>(this, std::move(callback));
  install_task->SetBookmarkAppHelper(std::move(bookmark_app_helper));
  install_task->SetWebContents(web_contents);

  // InstallTask is owned by the bind state and will be disposed in
  // DestroyInstallTask.
  helper->Create(base::BindRepeating(OnBookmarkAppInstalled,
                                     base::Passed(std::move(install_task))));
}

void BookmarkAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool no_network_install,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto bookmark_app_helper = std::make_unique<BookmarkAppHelper>(
      profile(), std::move(web_application_info), /*web_contents=*/nullptr,
      install_source);

  if (no_network_install) {
    // We should only install windowed apps via this method.
    bookmark_app_helper->set_forced_launch_type(extensions::LAUNCH_TYPE_WINDOW);
    bookmark_app_helper->set_is_no_network_install();
  }

  BookmarkAppHelper* helper = bookmark_app_helper.get();

  auto install_task = std::make_unique<InstallTask>(this, std::move(callback));
  install_task->SetBookmarkAppHelper(std::move(bookmark_app_helper));

  // InstallTask is owned by the bind state and will be disposed in
  // DestroyInstallTask.
  helper->Create(base::BindRepeating(OnBookmarkAppInstalled,
                                     base::Passed(std::move(install_task))));
}

void BookmarkAppInstallManager::InstallWebAppWithOptions(
    content::WebContents* web_contents,
    const web_app::ExternalInstallOptions& install_options,
    OnceInstallCallback callback) {
  auto data_retriever = data_retriever_factory_.Run();

  web_app::WebAppDataRetriever* data_retriever_ptr = data_retriever.get();

  auto install_task = std::make_unique<InstallTask>(this, std::move(callback));
  install_task->SetDataRetriever(std::move(data_retriever));
  install_task->SetWebContents(web_contents);

  // Unretained |this| is used here because |install_task| is
  // InstallManagerObserver which guarantees that it'll never run the callback
  // if |this| is destroyed.
  data_retriever_ptr->GetWebApplicationInfo(
      web_contents,
      base::BindOnce(OnGetWebApplicationInfo, base::Unretained(this),
                     std::move(install_task), install_options));
}

void BookmarkAppInstallManager::InstallOrUpdateWebAppFromSync(
    const web_app::AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  // |callback| is ignored here: the legacy system doesn't report completion.
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile())->extension_service();
  DCHECK(extension_service);

  if (finalizer()->CanSkipAppUpdateForSync(app_id, *web_application_info))
    return;

#if defined(OS_CHROMEOS)
  // On Chrome OS, sync always locally installs an app.
  bool is_locally_installed = true;
#else
  bool is_locally_installed =
      extension_service->GetInstalledExtension(app_id) != nullptr;
#endif

  CreateOrUpdateBookmarkApp(extension_service, web_application_info.get(),
                            is_locally_installed);
}

void BookmarkAppInstallManager::InstallWebAppForTesting(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  // |callback| is ignored here: the legacy system doesn't report completion.
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile())->extension_service();
  DCHECK(extension_service);

  CreateOrUpdateBookmarkApp(extension_service, web_application_info.get(),
                            true /*is_locally_installed*/);
}

void BookmarkAppInstallManager::SetBookmarkAppHelperFactoryForTesting(
    BookmarkAppHelperFactory bookmark_app_helper_factory) {
  bookmark_app_helper_factory_ = std::move(bookmark_app_helper_factory);
}

void BookmarkAppInstallManager::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

}  // namespace extensions
