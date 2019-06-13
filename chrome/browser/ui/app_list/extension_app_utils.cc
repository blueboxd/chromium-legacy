// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_utils.h"

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/vector_icons.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils_chromeos.h"
#endif

namespace app_list {

namespace {

constexpr char const* kAppIdsHiddenInLauncher[] = {
    extension_misc::kChromeCameraAppId};

}  // namespace

bool ShouldShowInLauncher(const extensions::Extension* extension,
                          content::BrowserContext* context) {
  // TODO(crbug.com/971029): Make this a per System Web App property that is
  // accessible by querying the SystemWebAppManager.
#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(context);
  // These System Web Apps should not show in the launcher as they are added
  // as internal apps.
  web_app::SystemAppType hidden_system_web_apps[] = {
      // TODO(crbug.com/836128): Remove this once OS Settings is launched, and
      // permanently migrate OS Settings from an internal app to a full System
      // Web App.
      web_app::SystemAppType::SETTINGS,
  };
  for (auto app_type : hidden_system_web_apps) {
    if (extension->id() ==
        web_app::GetAppIdForSystemWebApp(profile, app_type)) {
      return false;
    }
  }
#endif

  return !HideInLauncherById(extension->id()) &&
         chromeos::DemoSession::ShouldDisplayInAppLauncher(extension->id()) &&
         extensions::ui_util::ShouldDisplayInAppLauncher(extension, context);
}

bool HideInLauncherById(std::string extension_id) {
  for (auto* const id : kAppIdsHiddenInLauncher) {
    if (id == extension_id) {
      return true;
    }
  }
  return false;
}

void AddMenuItemIconsForSystemApps(const std::string& app_id,
                                   ui::SimpleMenuModel* menu_model,
                                   int start_index,
                                   int count) {
  if (app_id != extension_misc::kFilesManagerAppId)
    return;

  for (int i = 0; i < count; ++i) {
    const int index = start_index + i;
    if (menu_model->GetLabelAt(index) ==
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW)) {
      const views::MenuConfig& menu_config = views::MenuConfig::instance();
      menu_model->SetIcon(
          index, gfx::Image(gfx::CreateVectorIcon(
                     views::kNewWindowIcon, menu_config.touchable_icon_size,
                     menu_config.touchable_icon_color)));
    }
  }
}

}  // namespace app_list
