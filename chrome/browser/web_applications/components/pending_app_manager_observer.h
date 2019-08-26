// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_OBSERVER_H_

namespace web_app {

enum class RegistrationResultCode { kSuccess, kAlreadyRegistered, kTimeout };

class PendingAppManagerObserver : public base::CheckedObserver {
 public:
  virtual void OnRegistrationFinished(const GURL& launch_url,
                                      RegistrationResultCode result) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_OBSERVER_H_
