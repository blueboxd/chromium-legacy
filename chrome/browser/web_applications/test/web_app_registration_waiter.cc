// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_registration_waiter.h"

namespace web_app {

WebAppRegistrationWaiter::WebAppRegistrationWaiter(PendingAppManager* manager) {
  observer_.Add(manager);
}

WebAppRegistrationWaiter::~WebAppRegistrationWaiter() = default;

void WebAppRegistrationWaiter::AwaitNextRegistration(
    const GURL& launch_url,
    RegistrationResultCode result) {
  launch_url_ = launch_url;
  result_ = result;
  run_loop_.Run();
}

void WebAppRegistrationWaiter::OnRegistrationFinished(
    const GURL& launch_url,
    RegistrationResultCode result) {
  CHECK_EQ(launch_url_, launch_url);
  CHECK_EQ(result_, result);
  run_loop_.Quit();
}

}  // namespace web_app
