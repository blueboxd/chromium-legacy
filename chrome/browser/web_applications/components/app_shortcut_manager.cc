// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_shortcut_manager.h"

namespace web_app {

AppShortcutManager::AppShortcutManager(Profile* profile) : profile_(profile) {}

AppShortcutManager::~AppShortcutManager() = default;

void AppShortcutManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
}

}  // namespace web_app
