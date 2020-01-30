// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_shelf_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/crostini/crostini_app_restart_view.h"
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

web_app::DisplayMode ConvertLaunchTypeCommandToDisplayMode(int command_id) {
  switch (command_id) {
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      return web_app::DisplayMode::kBrowser;
    case ash::LAUNCH_TYPE_WINDOW:
      return web_app::DisplayMode::kStandalone;
    default:
      return web_app::DisplayMode::kUndefined;
  }
}

extensions::LaunchType ConvertLaunchTypeCommandToExtensionLaunchType(
    int command_id) {
  switch (command_id) {
    case ash::LAUNCH_TYPE_PINNED_TAB:
      return extensions::LAUNCH_TYPE_PINNED;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      return extensions::LAUNCH_TYPE_REGULAR;
    case ash::LAUNCH_TYPE_WINDOW:
      return extensions::LAUNCH_TYPE_WINDOW;
    case ash::LAUNCH_TYPE_FULLSCREEN:
      return extensions::LAUNCH_TYPE_FULLSCREEN;
    default:
      return extensions::LAUNCH_TYPE_INVALID;
  }
}

}  // namespace

AppServiceShelfContextMenu::AppServiceShelfContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : ShelfContextMenu(controller, item, display_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(controller->profile());
  DCHECK(proxy);
  app_type_ = proxy->AppRegistryCache().GetAppType(item->id.app_id);
}

AppServiceShelfContextMenu::~AppServiceShelfContextMenu() = default;

void AppServiceShelfContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(controller()->profile());
  DCHECK(proxy);
  proxy->GetMenuModel(
      item().id.app_id, apps::mojom::MenuType::kShelf, display_id(),
      base::BindOnce(&AppServiceShelfContextMenu::OnGetMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppServiceShelfContextMenu::ExecuteCommand(int command_id,
                                                int event_flags) {
  if (ExecuteCommonCommand(command_id, event_flags))
    return;

  switch (command_id) {
    case ash::SHOW_APP_INFO:
      ShowAppInfo();
      break;

    case ash::MENU_NEW_WINDOW:
      if (app_type_ == apps::mojom::AppType::kCrostini) {
        ShelfContextMenu::ExecuteCommand(ash::MENU_OPEN_NEW, event_flags);
        return;
      }
      chrome::NewEmptyWindow(controller()->profile());
      break;

    case ash::MENU_NEW_INCOGNITO_WINDOW:
      chrome::NewEmptyWindow(controller()->profile()->GetOffTheRecordProfile());
      break;

    case ash::STOP_APP:
      if (item().id.app_id == crostini::GetTerminalId()) {
        crostini::CrostiniManager::GetForProfile(controller()->profile())
            ->StopVm(crostini::kCrostiniDefaultVmName, base::DoNothing());
      } else if (item().id.app_id == plugin_vm::kPluginVmAppId) {
        plugin_vm::PluginVmManager::GetForProfile(controller()->profile())
            ->StopPluginVm(plugin_vm::kPluginVmName);
      } else {
        LOG(ERROR) << "App " << item().id.app_id
                   << " should not have a stop app command.";
      }
      break;

    case ash::LAUNCH_TYPE_PINNED_TAB:
      FALLTHROUGH;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      FALLTHROUGH;
    case ash::LAUNCH_TYPE_WINDOW:
      FALLTHROUGH;
    case ash::LAUNCH_TYPE_FULLSCREEN:
      SetLaunchType(command_id);
      break;

    case ash::CROSTINI_USE_LOW_DENSITY:
    case ash::CROSTINI_USE_HIGH_DENSITY: {
      crostini::CrostiniRegistryService* registry_service =
          crostini::CrostiniRegistryServiceFactory::GetForProfile(
              controller()->profile());
      const bool scaled = command_id == ash::CROSTINI_USE_LOW_DENSITY;
      registry_service->SetAppScaled(item().id.app_id, scaled);
      if (controller()->IsOpen(item().id))
        CrostiniAppRestartView::Show(item().id, display_id());
      return;
    }

    default:
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

      ShelfContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

bool AppServiceShelfContextMenu::IsCommandIdChecked(int command_id) const {
  switch (app_type_) {
    case apps::mojom::AppType::kWeb:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsWithoutExtensions)) {
        if (command_id >= ash::LAUNCH_TYPE_PINNED_TAB &&
            command_id <= ash::LAUNCH_TYPE_WINDOW) {
          auto* provider =
              web_app::WebAppProvider::Get(controller()->profile());
          DCHECK(provider);
          web_app::DisplayMode effective_display_mode =
              provider->registrar().GetAppEffectiveDisplayMode(
                  item().id.app_id);
          return effective_display_mode != web_app::DisplayMode::kUndefined &&
                 effective_display_mode ==
                     ConvertLaunchTypeCommandToDisplayMode(command_id);
        }
        return ShelfContextMenu::IsCommandIdChecked(command_id);
      }
      // Otherwise deliberately fall through to fallback on Bookmark Apps.
      FALLTHROUGH;
    case apps::mojom::AppType::kExtension:
      if (command_id >= ash::LAUNCH_TYPE_PINNED_TAB &&
          command_id <= ash::LAUNCH_TYPE_WINDOW) {
        return GetExtensionLaunchType() ==
               ConvertLaunchTypeCommandToExtensionLaunchType(command_id);
      } else if (command_id < ash::COMMAND_ID_COUNT) {
        return ShelfContextMenu::IsCommandIdChecked(command_id);
      } else {
        return (extension_menu_items_ &&
                extension_menu_items_->IsCommandIdChecked(command_id));
      }
    case apps::mojom::AppType::kArc:
      FALLTHROUGH;
    case apps::mojom::AppType::kCrostini:
      FALLTHROUGH;
    case apps::mojom::AppType::kBuiltIn:
      FALLTHROUGH;
    default:
      return ShelfContextMenu::IsCommandIdChecked(command_id);
  }
}

bool AppServiceShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id < ash::COMMAND_ID_COUNT)
    return ShelfContextMenu::IsCommandIdEnabled(command_id);
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
      extension_menu_items_) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  }
  return true;
}

void AppServiceShelfContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback,
    apps::mojom::MenuItemsPtr menu_items) {
  auto menu_model = GetBaseMenuModel();
  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  size_t index = 0;
  // Unretained is safe here because PopulateNewItemFromMojoMenuItems should
  // call GetVectorIcon synchronously.
  if (apps::PopulateNewItemFromMojoMenuItems(
          menu_items->items, menu_model.get(), submenu_.get(),
          base::BindOnce(&AppServiceShelfContextMenu::GetCommandIdVectorIcon,
                         base::Unretained(this)))) {
    index = 1;
  }

  if (ShouldAddPinMenu())
    AddPinMenu(menu_model.get());

  for (size_t i = index; i < menu_items->items.size(); i++) {
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

  if (app_type_ == apps::mojom::AppType::kExtension)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShelfContextMenu::BuildExtensionAppShortcutsMenu(
    ui::SimpleMenuModel* menu_model) {
  extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      controller()->profile(), this, menu_model,
      base::Bind(MenuItemHasLauncherContext));

  int index = 0;
  extension_menu_items_->AppendExtensionItems(
      extensions::MenuItem::ExtensionKey(item().id.app_id), base::string16(),
      &index, false /*is_action_menu*/);

  app_list::AddMenuItemIconsForSystemApps(
      item().id.app_id, menu_model, menu_model->GetItemCount() - index, index);
}

void AppServiceShelfContextMenu::BuildArcAppShortcutsMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback) {
  const ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(item().id.app_id);
  if (!app_info) {
    LOG(ERROR) << "App " << item().id.app_id << " is not available.";
    std::move(callback).Run(std::move(menu_model));
    return;
  }

  arc_shortcuts_menu_builder_ =
      std::make_unique<arc::ArcAppShortcutsMenuBuilder>(
          controller()->profile(), item().id.app_id, display_id(),
          ash::LAUNCH_APP_SHORTCUT_FIRST, ash::LAUNCH_APP_SHORTCUT_LAST);
  arc_shortcuts_menu_builder_->BuildMenu(
      app_info->package_name, std::move(menu_model), std::move(callback));
}

void AppServiceShelfContextMenu::ShowAppInfo() {
  if (app_type_ == apps::mojom::AppType::kArc) {
    chrome::ShowAppManagementPage(controller()->profile(), item().id.app_id);
    base::UmaHistogramEnumeration(
        kAppManagementEntryPointsHistogramName,
        AppManagementEntryPoint::kShelfContextMenuAppInfoArc);
    return;
  }

  controller()->DoShowAppInfoFlow(controller()->profile(), item().id.app_id);
}

void AppServiceShelfContextMenu::SetLaunchType(int command_id) {
  switch (app_type_) {
    case apps::mojom::AppType::kWeb:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsWithoutExtensions)) {
        // Web apps can only toggle between kStandalone and kBrowser.
        web_app::DisplayMode user_display_mode =
            ConvertLaunchTypeCommandToDisplayMode(command_id);
        if (user_display_mode != web_app::DisplayMode::kUndefined) {
          auto* provider =
              web_app::WebAppProvider::Get(controller()->profile());
          DCHECK(provider);
          provider->registry_controller().SetAppUserDisplayMode(
              item().id.app_id, user_display_mode);
        }
        return;
      }
      // Otherwise deliberately fall through to fallback on Bookmark Apps.
      FALLTHROUGH;
    case apps::mojom::AppType::kExtension:
      SetExtensionLaunchType(command_id);
      return;
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

void AppServiceShelfContextMenu::SetExtensionLaunchType(int command_id) {
  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::LAUNCH_TYPE_PINNED_TAB:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_PINNED);
      break;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_REGULAR);
      break;
    case ash::LAUNCH_TYPE_WINDOW: {
      // Hosted apps can only toggle between LAUNCH_WINDOW and LAUNCH_REGULAR.
      extensions::LaunchType launch_type =
          GetExtensionLaunchType() == extensions::LAUNCH_TYPE_WINDOW
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;

      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                launch_type);
      break;
    }
    case ash::LAUNCH_TYPE_FULLSCREEN:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_FULLSCREEN);
      break;
    default:
      return;
  }
}

extensions::LaunchType AppServiceShelfContextMenu::GetExtensionLaunchType()
    const {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller()->profile())
          ->GetExtensionById(item().id.app_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(
      extensions::ExtensionPrefs::Get(controller()->profile()), extension);
}

bool AppServiceShelfContextMenu::ShouldAddPinMenu() {
  switch (app_type_) {
    case apps::mojom::AppType::kArc: {
      const arc::ArcAppShelfId& app_id =
          arc::ArcAppShelfId::FromString(item().id.app_id);
      const ArcAppListPrefs* arc_list_prefs =
          ArcAppListPrefs::Get(controller()->profile());
      DCHECK(arc_list_prefs);
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          arc_list_prefs->GetApp(app_id.app_id());
      if (!app_id.has_shelf_group_id() && app_info->launchable)
        return true;
      return false;
    }
    case apps::mojom::AppType::kBuiltIn: {
      bool show_in_launcher = false;
      apps::AppServiceProxy* proxy =
          apps::AppServiceProxyFactory::GetForProfile(controller()->profile());
      DCHECK(proxy);
      proxy->AppRegistryCache().ForOneApp(
          item().id.app_id, [&show_in_launcher](const apps::AppUpdate& update) {
            if (update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue)
              show_in_launcher = true;
          });
      return show_in_launcher;
    }
    case apps::mojom::AppType::kCrostini:
      FALLTHROUGH;
    case apps::mojom::AppType::kExtension:
      FALLTHROUGH;
    case apps::mojom::AppType::kWeb:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}
