// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_

#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

void AddCommandItem(uint32_t command_id,
                    uint32_t string_id,
                    apps::mojom::MenuItemsPtr* menu_items);

apps::mojom::MenuItemPtr CreateRadioItem(uint32_t command_id,
                                         uint32_t string_id,
                                         int group_id);

void AddRadioItem(uint32_t command_id,
                  uint32_t string_id,
                  int group_id,
                  apps::mojom::MenuItemsPtr* menu_items);

void CreateOpenNewSubmenu(uint32_t string_id,
                          apps::mojom::MenuItemsPtr* menu_items);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_
