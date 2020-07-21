// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

// This is adapted from base/barrier_closure.cc. os_hooks_results is maintained
// to track install results from different OS hooks callers
class OsHooksBarrierInfo {
 public:
  explicit OsHooksBarrierInfo(InstallOsHooksCallback done_callback)
      : done_callback_(std::move(done_callback)) {}

  void Run(OsHookType::Type os_hook, bool created) {
    DCHECK(!os_hooks_called_[os_hook]);

    os_hooks_called_[os_hook] = true;
    os_hooks_results_[os_hook] = created;

    if (os_hooks_called_.all()) {
      std::move(done_callback_).Run(os_hooks_results_);
    }
  }

 private:
  OsHooksResults os_hooks_results_{false};
  std::bitset<OsHookType::kMaxValue + 1> os_hooks_called_{false};
  InstallOsHooksCallback done_callback_;
};

OsIntegrationManager::OsIntegrationManager() = default;

OsIntegrationManager::~OsIntegrationManager() = default;

void OsIntegrationManager::SetSubsystems(
    AppShortcutManager* shortcut_manager,
    FileHandlerManager* file_handler_manager,
    WebAppUiManager* ui_manager) {
  shortcut_manager_ = shortcut_manager;
  file_handler_manager_ = file_handler_manager;
  ui_manager_ = ui_manager;
}

void OsIntegrationManager::SuppressOsHooksForTesting() {
  suppress_os_hooks_for_testing_ = true;
}

void OsIntegrationManager::InstallOsHooks(
    const AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options) {
  DCHECK(shortcut_manager_);

  if (suppress_os_hooks_for_testing_) {
    OsHooksResults os_hooks_results{true};
    std::move(callback).Run(os_hooks_results);
    return;
  }
  // Note: This barrier protects against multiple calls on the same type, but
  // it doesn't protect against the case where we fail to call Run / create a
  // callback for every type. Developers should double check that Run is
  // called for every OsHookType::Type. If there is any missing type, the
  // InstallOsHooksCallback will not get run.
  base::RepeatingCallback<void(OsHookType::Type os_hook, bool created)>
      barrier = base::BindRepeating(
          &OsHooksBarrierInfo::Run,
          base::Owned(new OsHooksBarrierInfo(std::move(callback))));

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (options.add_to_applications_menu &&
      shortcut_manager_->CanCreateShortcuts()) {
    shortcut_manager_->CreateShortcuts(
        app_id, options.add_to_desktop,
        base::BindOnce(&OsIntegrationManager::OnShortcutsCreated,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(web_app_info), std::move(options), barrier));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&OsIntegrationManager::OnShortcutsCreated,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(web_app_info), std::move(options), barrier,
                       /*shortcuts_created=*/false));
  }
}

void OsIntegrationManager::OnShortcutsCreated(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options,
    base::RepeatingCallback<void(OsHookType::Type os_hook, bool created)>
        barrier_callback,
    bool shortcuts_created) {
  DCHECK(file_handler_manager_);
  DCHECK(ui_manager_);

  barrier_callback.Run(OsHookType::kShortcuts, true);

  // TODO(crbug.com/1087219): callback should be run after all hooks are
  // deployed, need to refactor filehandler to allow this.
  file_handler_manager_->EnableAndRegisterOsFileHandlers(app_id);
  barrier_callback.Run(OsHookType::kFileHandlers, true);

  if (options.add_to_quick_launch_bar &&
      ui_manager_->CanAddAppToQuickLaunchBar()) {
    ui_manager_->AddAppToQuickLaunchBar(app_id);
  }
  if (shortcuts_created) {
    if (web_app_info) {
      shortcut_manager_->RegisterShortcutsMenuWithOs(
          app_id, web_app_info->shortcut_infos,
          web_app_info->shortcuts_menu_icons_bitmaps);
      // TODO(https://crbug.com/1098471): fix RegisterShortcutsMenuWithOs to
      // take callback.
      barrier_callback.Run(OsHookType::kShortcutsMenu, true);
    } else {
      shortcut_manager_->ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
          app_id, base::BindOnce(barrier_callback, OsHookType::kShortcutsMenu));
    }
  } else {
    barrier_callback.Run(OsHookType::kShortcutsMenu, false);
  }

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      options.run_on_os_login) {
    // TODO(crbug.com/897302): Add run on OS login dev activation from
    // manifest, for now it is on by default if feature flag is enabled.
    shortcut_manager_->RegisterRunOnOsLogin(
        app_id, base::BindOnce(barrier_callback, OsHookType::kRunOnOsLogin));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(barrier_callback, OsHookType::kRunOnOsLogin, false));
  }
}

}  // namespace web_app
