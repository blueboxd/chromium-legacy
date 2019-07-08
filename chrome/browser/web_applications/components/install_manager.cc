// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_manager.h"

#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/install_manager_observer.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

void OnInstallabilityCheckCompleted(
    InstallManager::WebAppInstallabilityCheckCallback callback,
    std::unique_ptr<content::WebContents> web_contents,
    const InstallableData& data) {
  // This task is posted. This is because this function will be called by the
  // InstallableManager associated with |web_contents|, and |web_contents| might
  // be freed in the callback. If that happens, and this isn't posted, the
  // InstallableManager will be freed, and then when this function returns to
  // the InstallManager calling function, there will be a crash.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(web_contents),
                                data.errors.empty()));
}

void OnWebAppUrlLoaded(
    InstallManager::WebAppInstallabilityCheckCallback callback,
    std::unique_ptr<content::WebContents> web_contents,
    WebAppUrlLoader::Result result) {
  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    std::move(callback).Run(std::move(web_contents), false);
    return;
  }

  InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.has_worker = true;
  params.wait_for_worker = true;
  InstallableManager::FromWebContents(web_contents.get())
      ->GetData(params,
                base::BindOnce(&OnInstallabilityCheckCompleted,
                               std::move(callback), std::move(web_contents)));
}

}  // namespace

InstallManager::InstallManager(Profile* profile) : profile_(profile) {}

InstallManager::~InstallManager() = default;

void InstallManager::SetSubsystems(AppRegistrar* registrar,
                                   InstallFinalizer* finalizer) {
  registrar_ = registrar;
  finalizer_ = finalizer;
}

void InstallManager::Shutdown() {
  for (InstallManagerObserver& observer : observers_)
    observer.OnInstallManagerShutdown();
}

void InstallManager::LoadWebAppAndCheckInstallability(
    const GURL& web_app_url,
    WebAppInstallabilityCheckCallback callback) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  InstallableManager::CreateForWebContents(web_contents.get());
  SecurityStateTabHelper::CreateForWebContents(web_contents.get());

  // Grab WebContents pointer now, before the call to BindOnce might null out
  // |web_contents|.
  content::WebContents* web_contents_ptr = web_contents.get();
  url_loader_.LoadUrl(web_app_url, web_contents_ptr,
                      base::BindOnce(&OnWebAppUrlLoaded, std::move(callback),
                                     std::move(web_contents)));
}

void InstallManager::AddObserver(InstallManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void InstallManager::RemoveObserver(InstallManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace web_app
