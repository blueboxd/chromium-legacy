// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_uma.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

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

AppServiceContextMenu::AppServiceContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller)
    : AppContextMenu(delegate, profile, app_id, controller) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  app_type_ = proxy->AppRegistryCache().GetAppType(app_id);
}

AppServiceContextMenu::~AppServiceContextMenu() = default;

void AppServiceContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  DCHECK(proxy);
  proxy->GetMenuModel(
      app_id(), apps::mojom::MenuType::kAppList,
      controller()->GetAppListDisplayId(),
      base::BindOnce(&AppServiceContextMenu::OnGetMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppServiceContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::LAUNCH_NEW:
      delegate()->ExecuteLaunchCommand(event_flags);
      break;

    case ash::SHOW_APP_INFO:
      ShowAppInfo();
      break;

    case ash::OPTIONS:
      controller()->ShowOptionsPage(profile(), app_id());
      break;

    case ash::UNINSTALL:
      controller()->UninstallApp(profile(), app_id());
      break;

    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      controller()->CreateNewWindow(profile(), false);
      break;

    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      controller()->CreateNewWindow(profile(), true);
      break;

    case ash::STOP_APP:
      if (app_id() == crostini::GetTerminalId()) {
        crostini::CrostiniManager::GetForProfile(profile())->StopVm(
            crostini::kCrostiniDefaultVmName, base::DoNothing());
      } else if (app_id() == plugin_vm::kPluginVmAppId) {
        plugin_vm::PluginVmManager::GetForProfile(profile())->StopPluginVm(
            plugin_vm::kPluginVmName);
      } else {
        LOG(ERROR) << "App " << app_id()
                   << " should not have a stop app command.";
      }
      break;

    default:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        SetLaunchType(command_id);
        return;
      }

      if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
              command_id)) {
        extension_menu_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                              content::ContextMenuParams());
        return;
      }

      if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
          command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
        DCHECK(arc_shortcuts_menu_builder_);
        arc_shortcuts_menu_builder_->ExecuteCommand(command_id);
        return;
      }

      AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

bool AppServiceContextMenu::IsCommandIdChecked(int command_id) const {
  switch (app_type_) {
    case apps::mojom::AppType::kWeb:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsWithoutExtensions)) {
        if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
            command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
          auto* provider = web_app::WebAppProvider::Get(profile());
          DCHECK(provider);
          web_app::DisplayMode effective_display_mode =
              provider->registrar().GetAppEffectiveDisplayMode(app_id());
          return effective_display_mode != web_app::DisplayMode::kUndefined &&
                 effective_display_mode ==
                     ConvertUseLaunchTypeCommandToDisplayMode(command_id);
        }
        return AppContextMenu::IsCommandIdChecked(command_id);
      }
      // Otherwise deliberately fall through to fallback on Bookmark Apps.
      FALLTHROUGH;
    case apps::mojom::AppType::kExtension:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        return static_cast<int>(
                   controller()->GetExtensionLaunchType(profile(), app_id())) +
                   ash::USE_LAUNCH_TYPE_COMMAND_START ==
               command_id;
      } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                     command_id)) {
        return extension_menu_items_->IsCommandIdChecked(command_id);
      }
      return AppContextMenu::IsCommandIdChecked(command_id);

    case apps::mojom::AppType::kArc:
      FALLTHROUGH;
    case apps::mojom::AppType::kCrostini:
      FALLTHROUGH;
    case apps::mojom::AppType::kBuiltIn:
      FALLTHROUGH;
    default:
      return AppContextMenu::IsCommandIdChecked(command_id);
  }
}

bool AppServiceContextMenu::IsCommandIdEnabled(int command_id) const {
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
      extension_menu_items_) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

void AppServiceContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback,
    apps::mojom::MenuItemsPtr menu_items) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  int index = 0;
  // Unretained is safe here because PopulateNewItemFromMojoMenuItems should
  // call GetVectorIcon synchronously.
  if (apps::PopulateNewItemFromMojoMenuItems(
          menu_items->items, menu_model.get(), submenu_.get(),
          base::BindOnce(&AppServiceContextMenu::GetVectorIcon,
                         base::Unretained(this)))) {
    index = 1;
  }

  // Create default items.
  if (app_id() != extension_misc::kChromeAppId)
    app_list::AppContextMenu::BuildMenu(menu_model.get());

  if (app_type_ == apps::mojom::AppType::kExtension) {
    BuildExtensionAppShortcutsMenu(menu_model.get());
  }

  for (int i = index; i < static_cast<int>(menu_items->items.size()); i++) {
    DCHECK_EQ(apps::mojom::MenuItemType::kCommand, menu_items->items[i]->type);
    AddContextMenuOption(
        menu_model.get(),
        static_cast<ash::CommandId>(menu_items->items[i]->command_id),
        menu_items->items[i]->string_id);
  }

  if (app_type_ == apps::mojom::AppType::kArc) {
    BuildArcAppShortcutsMenu(std::move(menu_model), std::move(callback));
    return;
  }

  std::move(callback).Run(std::move(menu_model));
}

const gfx::VectorIcon& AppServiceContextMenu::GetVectorIcon(int command_id,
                                                            int string_id) {
  return GetMenuItemVectorIcon(command_id, string_id);
}

void AppServiceContextMenu::BuildExtensionAppShortcutsMenu(
    ui::SimpleMenuModel* menu_model) {
  extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      profile(), this, menu_model, base::Bind(MenuItemHasLauncherContext));

  // Assign unique IDs to commands added by the app itself.
  int index = ash::USE_LAUNCH_TYPE_COMMAND_END;
  extension_menu_items_->AppendExtensionItems(
      extensions::MenuItem::ExtensionKey(app_id()), base::string16(), &index,
      false /*is_action_menu*/);

  const int appended_count = index - ash::USE_LAUNCH_TYPE_COMMAND_END;
  app_list::AddMenuItemIconsForSystemApps(
      app_id(), menu_model, menu_model->GetItemCount() - appended_count,
      appended_count);
}

void AppServiceContextMenu::BuildArcAppShortcutsMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    LOG(ERROR) << "App " << app_id() << " is not available.";
    std::move(callback).Run(std::move(menu_model));
    return;
  }

  arc_shortcuts_menu_builder_ =
      std::make_unique<arc::ArcAppShortcutsMenuBuilder>(
          profile(), app_id(), controller()->GetAppListDisplayId(),
          ash::LAUNCH_APP_SHORTCUT_FIRST, ash::LAUNCH_APP_SHORTCUT_LAST);
  arc_shortcuts_menu_builder_->BuildMenu(
      app_info->package_name, std::move(menu_model), std::move(callback));
}

void AppServiceContextMenu::ShowAppInfo() {
  if (app_type_ == apps::mojom::AppType::kArc) {
    chrome::ShowAppManagementPage(profile(), app_id());
    base::UmaHistogramEnumeration(
        kAppManagementEntryPointsHistogramName,
        AppManagementEntryPoint::kAppListContextMenuAppInfoArc);
    return;
  }

  controller()->DoShowAppInfoFlow(profile(), app_id());
}

void AppServiceContextMenu::SetLaunchType(int command_id) {
  switch (app_type_) {
    case apps::mojom::AppType::kWeb:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsWithoutExtensions)) {
        // Web apps can only toggle between kStandalone and kBrowser.
        web_app::DisplayMode user_display_mode =
            ConvertUseLaunchTypeCommandToDisplayMode(command_id);
        if (user_display_mode != web_app::DisplayMode::kUndefined) {
          auto* provider = web_app::WebAppProvider::Get(profile());
          DCHECK(provider);
          provider->registry_controller().SetAppUserDisplayMode(
              app_id(), user_display_mode);
        }
        return;
      }
      // Otherwise deliberately fall through to fallback on Bookmark Apps.
      FALLTHROUGH;
    case apps::mojom::AppType::kExtension: {
      // Hosted apps can only toggle between LAUNCH_TYPE_WINDOW and
      // LAUNCH_TYPE_REGULAR.
      extensions::LaunchType launch_type =
          (controller()->GetExtensionLaunchType(profile(), app_id()) ==
           extensions::LAUNCH_TYPE_WINDOW)
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;
      controller()->SetExtensionLaunchType(profile(), app_id(), launch_type);
      return;
    }
    case apps::mojom::AppType::kArc:
      FALLTHROUGH;
    case apps::mojom::AppType::kCrostini:
      FALLTHROUGH;
    case apps::mojom::AppType::kBuiltIn:
      FALLTHROUGH;
    default:
      return;
  }
}
