// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_UI_DELEGATE_H_

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

// Pure virtual interface used to perform Web App UI operations or listen to Web
// App UI events.
class WebAppUiDelegate {
 public:
  virtual size_t GetNumWindowsForApp(const AppId& app_id) = 0;

  virtual void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                           base::OnceClosure callback) = 0;

  // Migrates an app's OS attributes (e.g pin position, app list
  // folder/position, shortcuts).
  virtual void MigrateOSAttributes(const AppId& from, const AppId& to) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_UI_DELEGATE_H_
