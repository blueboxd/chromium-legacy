// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_

#include <memory>
#include <vector>

namespace web_app {

class WebApp;

// WebAppSyncBridge delegates sync-initiated installs and uninstalls using
// this interface.
class SyncInstallDelegate {
 public:
  virtual ~SyncInstallDelegate() = default;

  // |web_apps| are already registered and owned by the registrar.
  virtual void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps) = 0;
  // |web_apps| are already unregistered and not owned by the registrar.
  virtual void UninstallWebAppsAfterSync(
      std::vector<std::unique_ptr<WebApp>> web_apps) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
