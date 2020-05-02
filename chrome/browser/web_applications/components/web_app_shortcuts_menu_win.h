// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUTS_MENU_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUTS_MENU_WIN_H_

#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"

namespace web_app {

namespace internals {

// Deletes all .ico shortcuts menu icons that were written to disk at PWA
// install time. Call this when PWA is uninstalled on Windows.
void DeleteShortcutsMenuIcons(const base::FilePath& web_app_path);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUTS_MENU_WIN_H_
