// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/lacros_browser_shelf_item_delegate.h"

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/chromeos/lacros/lacros_loader.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/constants.h"

LacrosBrowserShelfItemDelegate::LacrosBrowserShelfItemDelegate()
    : ash::ShelfItemDelegate(ash::ShelfID(extension_misc::kLacrosAppId)) {}

LacrosBrowserShelfItemDelegate::~LacrosBrowserShelfItemDelegate() = default;

void LacrosBrowserShelfItemDelegate::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  // TODO(lacros): Handle window activation, window minimize, and spawning a
  // menu with a list of browser windows.
  LacrosLoader::Get()->Start();
  std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
}

void LacrosBrowserShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                                    int64_t command_id,
                                                    int32_t event_flags,
                                                    int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  // TODO(lacros): Handle custom context menu commands.
}

void LacrosBrowserShelfItemDelegate::Close() {
  // TODO(lacros): Close all browser windows.
}
