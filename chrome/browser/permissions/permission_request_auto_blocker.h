// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_AUTO_BLOCKER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_AUTO_BLOCKER_H_

#include "chrome/browser/permissions/permission_request.h"

// Describes the interface that needs to be implemented by a permission request
// automatic decision maker.
class PermissionRequestAutoBlocker {
 public:
  enum Response {
    USE_NORMAL_UI,
    USE_QUIET_UI,
    TIMEOUT,
  };
  using DecisionMadeCallback = base::OnceCallback<void(Response)>;

  virtual ~PermissionRequestAutoBlocker() = default;

  // Callback needs to be always be invoked, even when a decision could not be
  // made for some reason or another.
  virtual void MakeDecision(PermissionRequest* request,
                            DecisionMadeCallback callback) = 0;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_AUTO_BLOCKER_H_
