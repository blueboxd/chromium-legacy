// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/test/test_install_finalizer.h"

#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "components/crx_file/id_util.h"

namespace web_app {

// static
AppId TestInstallFinalizer::GetAppIdForUrl(const GURL& url) {
  return GenerateAppIdFromURL(url);
}

TestInstallFinalizer::TestInstallFinalizer() {}

TestInstallFinalizer::~TestInstallFinalizer() = default;

void TestInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  AppId app_id = GetAppIdForUrl(web_app_info.app_url);
  if (next_app_id_.has_value()) {
    app_id = next_app_id_.value();
    next_app_id_.reset();
  }

  InstallResultCode code = InstallResultCode::kSuccess;
  if (next_result_code_.has_value()) {
    code = next_result_code_.value();
    next_result_code_.reset();
  }

  // Store input data copies for inspecting in tests.
  web_app_info_copy_ = std::make_unique<WebApplicationInfo>(web_app_info);
  finalize_options_list_.push_back(options);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), app_id, code));
}

void TestInstallFinalizer::UninstallExternalWebApp(
    const GURL& app_url,
    UninstallExternalWebAppCallback callback) {
  DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
  uninstall_external_web_app_urls_.push_back(app_url);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [this, app_url, callback = std::move(callback)]() mutable {
                       bool result =
                           next_uninstall_external_web_app_results_[app_url];
                       next_uninstall_external_web_app_results_.erase(app_url);
                       std::move(callback).Run(result);
                     }));
}

bool TestInstallFinalizer::CanCreateOsShortcuts() const {
  return true;
}

void TestInstallFinalizer::CreateOsShortcuts(
    const AppId& app_id,
    bool add_to_desktop,
    CreateOsShortcutsCallback callback) {
  ++num_create_os_shortcuts_calls_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true /* shortcuts_created */));
}

bool TestInstallFinalizer::CanPinAppToShelf() const {
  return true;
}

void TestInstallFinalizer::PinAppToShelf(const AppId& app_id) {
  ++num_pin_app_to_shelf_calls_;
}

bool TestInstallFinalizer::CanReparentTab(const AppId& app_id,
                                          bool shortcut_created) const {
  return true;
}

void TestInstallFinalizer::ReparentTab(const AppId& app_id,
                                       content::WebContents* web_contents) {
  ++num_reparent_tab_calls_;
}

bool TestInstallFinalizer::CanRevealAppShim() const {
  return true;
}

void TestInstallFinalizer::RevealAppShim(const AppId& app_id) {
  ++num_reveal_appshim_calls_;
}

bool TestInstallFinalizer::CanSkipAppUpdateForSync(
    const AppId& app_id,
    const WebApplicationInfo& web_app_info) const {
  return next_can_skip_app_update_for_sync_;
}

void TestInstallFinalizer::SetNextFinalizeInstallResult(
    const AppId& app_id,
    InstallResultCode code) {
  next_app_id_ = app_id;
  next_result_code_ = code;
}

void TestInstallFinalizer::SetNextUninstallExternalWebAppResult(
    const GURL& app_url,
    bool uninstalled) {
  DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));
  next_uninstall_external_web_app_results_[app_url] = uninstalled;
}

void TestInstallFinalizer::SetNextCanSkipAppUpdateForSync(
    bool can_skip_app_update_for_sync) {
  next_can_skip_app_update_for_sync_ = can_skip_app_update_for_sync;
}

}  // namespace web_app
