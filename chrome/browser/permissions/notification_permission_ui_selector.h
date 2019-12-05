// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_NOTIFICATION_PERMISSION_UI_SELECTOR_H_

#include "chrome/browser/permissions/permission_request.h"

// The interface for implementations that decide if the quiet prompt UI should
// be used to display a notification permission |request|.
//
// Implementations of interface are expected to have long-lived instances that
// can support multiple requests, but only one at a time.
class NotificationPermissionUiSelector {
 public:
  enum class UiToUse {
    kNormalUi,
    kQuietUi,
  };

  enum class QuietUiReason {
    kEnabledInPrefs,
    kTriggeredByCrowdDeny,
  };

  using DecisionMadeCallback =
      base::OnceCallback<void(UiToUse, base::Optional<QuietUiReason>)>;

  virtual ~NotificationPermissionUiSelector() {}

  // Determines the UI to use for the given |request|, and invokes |callback|
  // when done, either synchronously or asynchrously. The |callback| is
  // guaranteed never to be invoked after |this| goes out of scope. Only one
  // request is supported at a time.
  virtual void SelectUiToUse(PermissionRequest* request,
                             DecisionMadeCallback callback) = 0;

  // Cancel the pending request, if any. After this, the |callback| is
  // guaranteed not to be invoked anymore, and another call to SelectUiToUse()
  // can be issued.
  virtual void Cancel() {}
};

#endif  // CHROME_BROWSER_PERMISSIONS_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
