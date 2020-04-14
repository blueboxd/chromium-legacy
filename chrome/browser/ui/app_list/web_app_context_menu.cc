// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/web_app_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/vector_icons.h"

namespace app_list {

namespace {

web_app::DisplayMode ConvertUseLaunchTypeCommandToDisplayMode(int command_id) {
  DCHECK(command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
         command_id < ash::USE_LAUNCH_TYPE_COMMAND_END);
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return web_app::DisplayMode::kBrowser;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return web_app::DisplayMode::kStandalone;
    case ash::USE_LAUNCH_TYPE_PINNED:
    case ash::USE_LAUNCH_TYPE_FULLSCREEN:
    default:
      return web_app::DisplayMode::kUndefined;
  }
}

}  // namespace

WebAppContextMenu::WebAppContextMenu(AppContextMenuDelegate* delegate,
                                     Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller)
    : AppContextMenu(delegate, profile, app_id, controller) {}

WebAppContextMenu::~WebAppContextMenu() = default;

int WebAppContextMenu::GetLaunchStringId() const {
  bool launch_in_window = IsCommandIdChecked(ash::USE_LAUNCH_TYPE_WINDOW);
  return launch_in_window ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
                          : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB;
}

void WebAppContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  if (!GetProvider().registrar().IsInstalled(app_id())) {
    std::move(callback).Run(nullptr);
    return;
  }

  AppContextMenu::GetMenuModel(std::move(callback));
}

void WebAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  const bool is_system_web_app =
      GetProvider().system_web_app_manager().IsSystemWebApp(app_id());

  if (!is_system_web_app)
    CreateOpenNewSubmenu(menu_model);

  AppContextMenu::BuildMenu(menu_model);

  AddContextMenuOption(menu_model, ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM);

  if (!is_system_web_app) {
    AddContextMenuOption(menu_model, ash::SHOW_APP_INFO,
                         IDS_APP_CONTEXT_MENU_SHOW_INFO);
  }
}

base::string16 WebAppContextMenu::GetLabelForCommandId(int command_id) const {
  if (command_id == ash::LAUNCH_NEW)
    return l10n_util::GetStringUTF16(GetLaunchStringId());

  return AppContextMenu::GetLabelForCommandId(command_id);
}

ui::ImageModel WebAppContextMenu::GetIconForCommandId(int command_id) const {
  if (command_id == ash::LAUNCH_NEW) {
    return ui::ImageModel::FromVectorIcon(
        GetMenuItemVectorIcon(ash::LAUNCH_NEW, GetLaunchStringId()));
  }

  return AppContextMenu::GetIconForCommandId(command_id);
}

bool WebAppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == ash::LAUNCH_NEW ||
         AppContextMenu::IsItemForCommandIdDynamic(command_id);
}

bool WebAppContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
      command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
    web_app::DisplayMode effective_display_mode =
        GetProvider().registrar().GetAppEffectiveDisplayMode(app_id());
    return effective_display_mode != web_app::DisplayMode::kUndefined &&
           effective_display_mode ==
               ConvertUseLaunchTypeCommandToDisplayMode(command_id);
  }
  return AppContextMenu::IsCommandIdChecked(command_id);
}

bool WebAppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::UNINSTALL) {
    return GetProvider().install_finalizer().CanUserUninstallExternalApp(
        app_id());
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

void WebAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == ash::LAUNCH_NEW) {
    delegate()->ExecuteLaunchCommand(event_flags);
  } else if (command_id == ash::SHOW_APP_INFO) {
    controller()->DoShowAppInfoFlow(profile(), app_id());
  } else if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
             command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
    // Web apps can only toggle between kStandalone and kBrowser.
    web_app::DisplayMode user_display_mode =
        ConvertUseLaunchTypeCommandToDisplayMode(command_id);
    if (user_display_mode != web_app::DisplayMode::kUndefined) {
      GetProvider().registry_controller().SetAppUserDisplayMode(
          app_id(), user_display_mode);
    }
  } else if (command_id == ash::UNINSTALL) {
    controller()->UninstallApp(profile(), app_id());
  } else {
    AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

void WebAppContextMenu::CreateOpenNewSubmenu(ui::SimpleMenuModel* menu_model) {
  // Touchable app context menus use an actionable submenu for LAUNCH_NEW.
  const int kGroupId = 1;
  open_new_submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::USE_LAUNCH_TYPE_REGULAR, IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
      kGroupId);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::USE_LAUNCH_TYPE_WINDOW, IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
      kGroupId);
  menu_model->AddActionableSubmenuWithStringIdAndIcon(
      ash::LAUNCH_NEW, GetLaunchStringId(), open_new_submenu_model_.get(),
      ui::ImageModel::FromVectorIcon(
          GetMenuItemVectorIcon(ash::LAUNCH_NEW, GetLaunchStringId())));
}

web_app::WebAppProvider& WebAppContextMenu::GetProvider() const {
  auto* provider = web_app::WebAppProvider::Get(profile());
  DCHECK(provider);
  return *provider;
}

}  // namespace app_list
