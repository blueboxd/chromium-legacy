// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_device_service.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/browser/service_process_host.h"

namespace content {

namespace {

base::RepeatingClosure& GetStartupCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

}  // namespace

const mojo::Remote<device::mojom::XRDeviceService>& GetXRDeviceService() {
  static base::NoDestructor<mojo::Remote<device::mojom::XRDeviceService>>
      remote;
  if (!*remote) {
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
#if defined(OS_WIN)
            .WithSandboxType(service_manager::SandboxType::kXrCompositing)
#else
            .WithSandboxType(service_manager::SandboxType::kUtility)
#endif
            .WithDisplayName("Isolated XR Device Service")
            .Pass());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) or goes idle for a short period of time -- meaning there
    // are no in-flight messages and no other interfaces bound through this
    // one -- then we will reset |remote|, causing the service process to be
    // terminated if it isn't already.
    remote->reset_on_disconnect();
    remote->reset_on_idle_timeout(base::TimeDelta::FromSeconds(5));

    auto& startup_callback = GetStartupCallback();
    if (startup_callback)
      startup_callback.Run();
  }

  return *remote;
}

void SetXRDeviceServiceStartupCallbackForTestingInternal(
    base::RepeatingClosure callback) {
  GetStartupCallback() = std::move(callback);
}

}  // namespace content
