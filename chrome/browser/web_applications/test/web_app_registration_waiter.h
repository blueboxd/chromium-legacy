// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/pending_app_manager_observer.h"

namespace web_app {

class WebAppRegistrationWaiter final : public PendingAppManagerObserver {
 public:
  explicit WebAppRegistrationWaiter(PendingAppManager* manager);
  ~WebAppRegistrationWaiter() override;

  void AwaitNextRegistration(const GURL& launch_url,
                             RegistrationResultCode result);

  // PendingAppManagerObserver:
  void OnRegistrationFinished(const GURL& launch_url,
                              RegistrationResultCode result) override;

 private:
  base::RunLoop run_loop_;
  GURL launch_url_;
  RegistrationResultCode result_;
  ScopedObserver<PendingAppManager, WebAppRegistrationWaiter> observer_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_
