// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "chrome/common/mac/app_shim_launch.h"

class AppShimHostBootstrap;

namespace apps {

// Registrar, and interface for services that can handle interactions with OSX
// shim processes.
class AppShimHandler {
 public:
  static AppShimHandler* Get();

  // Set or un-set the default handler.
  static void Set(AppShimHandler* handler);

  // Invoked by the AppShimHostBootstrap when a shim process has connected to
  // the browser process. This will connect to (creating, if needed) an
  // AppShimHost. |bootstrap| must have OnConnectedToHost or
  // OnFailedToConnectToHost called on it to inform the shim of the result.
  virtual void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) = 0;

 protected:
  AppShimHandler() {}
  virtual ~AppShimHandler() {}
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
