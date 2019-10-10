// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_manager.h"

namespace extensions {

BookmarkAppShortcutManager::BookmarkAppShortcutManager(Profile* profile)
    : web_app::AppShortcutManager(profile) {}

BookmarkAppShortcutManager::~BookmarkAppShortcutManager() = default;

}  // namespace extensions
