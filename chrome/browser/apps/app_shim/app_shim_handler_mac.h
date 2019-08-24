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

class AppShimHost;
class AppShimHostBootstrap;

namespace apps {

using ShimLaunchedCallback = base::OnceCallback<void(base::Process)>;
using ShimTerminatedCallback = base::OnceClosure;

// Registrar, and interface for services that can handle interactions with OSX
// shim processes.
class AppShimHandler {
 public:
  static AppShimHandler* Get();

  // Set or un-set the default handler.
  static void Set(AppShimHandler* handler);

  // Request that the handler launch the app shim process.
  virtual void OnShimLaunchRequested(
      AppShimHost* host,
      bool recreate_shims,
      apps::ShimLaunchedCallback launched_callback,
      apps::ShimTerminatedCallback terminated_callback) = 0;

  // Invoked by the AppShimHostBootstrap when a shim process has connected to
  // the browser process. This will connect to (creating, if needed) an
  // AppShimHost. |bootstrap| must have OnConnectedToHost or
  // OnFailedToConnectToHost called on it to inform the shim of the result.
  virtual void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) = 0;

  // Invoked by the shim host when the connection to the shim process is closed.
  virtual void OnShimClose(AppShimHost* host) = 0;

  // Invoked by the shim host when the shim process receives a focus event.
  // |files|, if non-empty, holds an array of files dragged onto the app bundle
  // or dock icon.
  virtual void OnShimFocus(AppShimHost* host,
                           AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) = 0;

  // Invoked by the shim host when the shim process is hidden or shown.
  virtual void OnShimSetHidden(AppShimHost* host, bool hidden) = 0;

  // Invoked by the shim host when the shim process receives a quit event.
  virtual void OnShimQuit(AppShimHost* host) = 0;

 protected:
  AppShimHandler() {}
  virtual ~AppShimHandler() {}
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
