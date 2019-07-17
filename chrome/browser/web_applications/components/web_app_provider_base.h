// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace web_app {

// Forward declarations of generalized interfaces.
class PendingAppManager;
class InstallManager;
class AppRegistrar;
class WebAppPolicyManager;
class WebAppUiManager;

class WebAppProviderBase : public KeyedService {
 public:
  static WebAppProviderBase* GetProviderBase(Profile* profile);

  WebAppProviderBase();
  ~WebAppProviderBase() override;

  // The app registry manager.
  virtual AppRegistrar& registrar() = 0;
  // UIs can use InstallManager for user-initiated Web Apps install.
  virtual InstallManager& install_manager() = 0;
  // Clients can use PendingAppManager to install, uninstall, and update
  // Web Apps.
  virtual PendingAppManager& pending_app_manager() = 0;
  // Clients can use WebAppPolicyManager to request updates of policy installed
  // Web Apps.
  // TODO(crbug.com/916381): Make a reference once WebAppPolicyManager is always
  // present. It's currently only present for Bookmark Apps.
  virtual WebAppPolicyManager* policy_manager() = 0;

  virtual WebAppUiManager& ui_manager() = 0;

  DISALLOW_COPY_AND_ASSIGN(WebAppProviderBase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_H_
