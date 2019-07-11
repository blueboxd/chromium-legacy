// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_REMOTE_DEBUGGING_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_REMOTE_DEBUGGING_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr_set.h>

#include "base/callback.h"
#include "base/optional.h"

// Initializes the DevTools service with the information provided on the
// command line.
class WebEngineRemoteDebugging {
 public:
  using OnListenCallback = base::OnceCallback<void(uint16_t port)>;
  using GetRemoteDebuggingPortCallback =
      fuchsia::web::Context::GetRemoteDebuggingPortCallback;

  WebEngineRemoteDebugging();
  ~WebEngineRemoteDebugging();

  // Called by the Context to signal a document has been loaded and signal to
  // the |debug_devtools_listeners_| that they can now successfully connect
  // ChromeDriver on |devtools_port_|.
  void OnDebugDevToolsPortReady();

  // Actual implementation of the Context.GetRemoteDebuggingPort() FIDL API.
  void GetRemoteDebuggingPort(GetRemoteDebuggingPortCallback callback);

 private:
  // Signals the remote debugging service status changed. |port| will be set to
  // 0 if the service was not open or failed to open.
  void OnRemoteDebuggingServiceStatusChanged(uint16_t port);

  void MaybeSendRemoteDebuggingCallbacks();

  // Set to true if remote debugging was enabled.
  bool remote_debugging_enabled_ = false;

  // The remote debugging port, 0 if not set or the service failed to open.
  uint16_t devtools_port_ = 0;

  // Set to true once OnRemoteDebuggingServiceStatusChanged() has been called.
  // Must only be set once.
  bool devtools_port_received_ = false;

  // Pending callbacks for the GetRemoteDebuggingPort() API.
  std::vector<GetRemoteDebuggingPortCallback> pending_callbacks_;

  // If remote debugging is enabled via the fuchsia.web.Debug API, holds the
  // Debug listeners.
  base::Optional<
      fidl::InterfacePtrSet<fuchsia::web::DevToolsPerContextListener>>
      debug_devtools_listeners_;

  // Set to true once the |debug_devtools_listeners_| have been notified.
  bool debug_devtools_listeners_notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebEngineRemoteDebugging);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_REMOTE_DEBUGGING_H_
