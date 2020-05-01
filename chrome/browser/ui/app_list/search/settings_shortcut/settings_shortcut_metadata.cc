// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_metadata.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"

namespace app_list {

namespace {

constexpr char kSettingsShortcutIDBluetooth[] = "settings://bluetooth";

}  // namespace

const std::vector<SettingsShortcut>& GetSettingsShortcutList() {
  static const base::NoDestructor<std::vector<SettingsShortcut>>
      settings_shortcuts(
          {{kSettingsShortcutIDBluetooth, IDS_LAUNCHER_SHORTCUTS_BLUETOOTH,
            vector_icons::kBluetoothIcon,
            chromeos::settings::mojom::kBluetoothDevicesSubpagePath,
            /*searchable_string_resource_id=*/0}});
  return *settings_shortcuts;
}

}  // namespace app_list
