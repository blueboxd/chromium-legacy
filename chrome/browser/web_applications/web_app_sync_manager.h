// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_MANAGER_H_

#include <memory>

#include "base/macros.h"

namespace web_app {

class WebAppSyncBridge;

// Exclusively used from the UI thread.
class WebAppSyncManager {
 public:
  WebAppSyncManager();
  ~WebAppSyncManager();

  WebAppSyncBridge& bridge() { return *bridge_; }

 private:
  std::unique_ptr<WebAppSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(WebAppSyncManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_MANAGER_H_
