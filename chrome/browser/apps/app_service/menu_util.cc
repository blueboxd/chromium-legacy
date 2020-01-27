// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/menu_util.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {
const int kInvalidRadioGroupId = -1;
const int kGroupId = 1;
}

namespace apps {

void AddCommandItem(uint32_t command_id,
                    uint32_t string_id,
                    apps::mojom::MenuItemsPtr* menu_items) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kCommand;
  menu_item->command_id = command_id;
  menu_item->string_id = string_id;
  menu_item->radio_group_id = kInvalidRadioGroupId;
  (*menu_items)->items.push_back(std::move(menu_item));
}

apps::mojom::MenuItemPtr CreateRadioItem(uint32_t command_id,
                                         uint32_t string_id,
                                         int group_id) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kRadio;
  menu_item->command_id = command_id;
  menu_item->string_id = string_id;
  menu_item->radio_group_id = group_id;
  return menu_item;
}

void AddRadioItem(uint32_t command_id,
                  uint32_t string_id,
                  int group_id,
                  apps::mojom::MenuItemsPtr* menu_items) {
  (*menu_items)
      ->items.push_back(CreateRadioItem(command_id, string_id, group_id));
}

void CreateOpenNewSubmenu(uint32_t string_id,
                          apps::mojom::MenuItemsPtr* menu_items) {
  apps::mojom::MenuItemPtr menu_item = apps::mojom::MenuItem::New();
  menu_item->type = apps::mojom::MenuItemType::kSubmenu;
  menu_item->command_id = ash::LAUNCH_NEW;
  menu_item->string_id = string_id;

  menu_item->submenu.push_back(
      CreateRadioItem(ash::USE_LAUNCH_TYPE_REGULAR,
                      IDS_APP_LIST_CONTEXT_MENU_NEW_TAB, kGroupId));
  menu_item->submenu.push_back(
      CreateRadioItem(ash::USE_LAUNCH_TYPE_WINDOW,
                      IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, kGroupId));

  menu_item->radio_group_id = kInvalidRadioGroupId;

  (*menu_items)->items.push_back(std::move(menu_item));
}

bool ShouldAddOpenItem(const std::string& app_id,
                       apps::mojom::MenuType menu_type,
                       Profile* profile) {
  if (menu_type != apps::mojom::MenuType::kShelf) {
    return false;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  if (proxy->InstanceRegistry().GetWindows(app_id).empty()) {
    return true;
  }

  return false;
}

bool ShouldAddCloseItem(const std::string& app_id,
                        apps::mojom::MenuType menu_type,
                        Profile* profile) {
  if (menu_type != apps::mojom::MenuType::kShelf) {
    return false;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  if (proxy->InstanceRegistry().GetWindows(app_id).empty()) {
    return false;
  }

  return true;
}

void PopulateRadioItemFromMojoMenuItems(
    const std::vector<apps::mojom::MenuItemPtr>& menu_items,
    ui::SimpleMenuModel* model) {
  for (auto& item : menu_items) {
    DCHECK_EQ(apps::mojom::MenuItemType::kRadio, item->type);
    model->AddRadioItem(item->command_id,
                        l10n_util::GetStringUTF16(item->string_id),
                        item->radio_group_id);
  }
}

bool PopulateNewItemFromMojoMenuItems(
    const std::vector<apps::mojom::MenuItemPtr>& menu_items,
    ui::SimpleMenuModel* model,
    ui::SimpleMenuModel* submenu,
    GetVectorIconCallback get_vector_icon) {
  if (menu_items.empty()) {
    return false;
  }

  auto& item = menu_items[0];
  if (item->command_id != ash::LAUNCH_NEW) {
    return false;
  }

  switch (item->type) {
    case apps::mojom::MenuItemType::kCommand: {
      const gfx::VectorIcon& icon =
          std::move(get_vector_icon).Run(item->command_id, item->string_id);
      model->AddItemWithStringIdAndIcon(item->command_id, item->string_id,
                                        icon);
      break;
    }
    case apps::mojom::MenuItemType::kRadio:
      NOTREACHED();
      return false;
    case apps::mojom::MenuItemType::kSubmenu:
      if (!item->submenu.empty()) {
        PopulateRadioItemFromMojoMenuItems(item->submenu, submenu);
        const gfx::VectorIcon& icon =
            std::move(get_vector_icon).Run(item->command_id, item->string_id);
        model->AddActionableSubmenuWithStringIdAndIcon(
            item->command_id, item->string_id, submenu, icon);
      }
      break;
  }
  return true;
}

}  // namespace apps
