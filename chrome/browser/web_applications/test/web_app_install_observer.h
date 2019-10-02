// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"

namespace web_app {

class WebAppInstallObserver final : public AppRegistrarObserver {
 public:
  explicit WebAppInstallObserver(Profile* profile);
  ~WebAppInstallObserver() override;

  AppId AwaitNextInstall();

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;

 private:
  base::RunLoop run_loop_;
  AppId app_id_;
  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallObserver);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
