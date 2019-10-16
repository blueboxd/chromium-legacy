// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;

namespace web_app {

class AppRegistrar;
struct ShortcutInfo;

// TODO(crbug.com/860581): Migrate functions from
// web_app_extension_shortcut.(h|cc) and
// platform_apps/shortcut_manager.(h|cc) to the AppShortcutManager, so web app
// shortcuts can be managed in an extensions agnostic way.
class AppShortcutManager {
 public:
  explicit AppShortcutManager(Profile* profile);
  virtual ~AppShortcutManager();

  void SetSubsystems(AppRegistrar* registrar);

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id|.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback) = 0;

 protected:
  AppRegistrar* registrar() { return registrar_; }
  Profile* profile() { return profile_; }

 private:
  AppRegistrar* registrar_ = nullptr;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(AppShortcutManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
