// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/manifest.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetXrDeviceServiceManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(device::mojom::kVrIsolatedServiceName)
          .WithDisplayName("XR Isolated Device Service")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("xr_compositing")
                  .Build())
          .ExposeCapability("xr_device_test_hook",
                            service_manager::Manifest::InterfaceList<
                                device_test::mojom::XRServiceTestHook>())
          .ExposeCapability("xr_device_provider",
                            service_manager::Manifest::InterfaceList<
                                device::mojom::IsolatedXRRuntimeProvider>())
          .Build()};
  return *manifest;
}
