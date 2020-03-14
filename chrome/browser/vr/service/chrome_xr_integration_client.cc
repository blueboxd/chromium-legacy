// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/chrome_xr_integration_client.h"

#include "build/build_config.h"
#include "content/public/browser/xr_install_helper.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/vr/gvr_install_helper.h"
#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"
#endif
#endif
namespace {
vr::ChromeXrIntegrationClient* g_instance = nullptr;
}

namespace vr {
ChromeXrIntegrationClient* ChromeXrIntegrationClient::GetInstance() {
  if (!g_instance)
    g_instance = new ChromeXrIntegrationClient();

  return g_instance;
}

std::unique_ptr<content::XrInstallHelper>
ChromeXrIntegrationClient::GetInstallHelper(
    device::mojom::XRDeviceId device_id) {
  switch (device_id) {
#if defined(OS_ANDROID)
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
      return std::make_unique<GvrInstallHelper>();
#if BUILDFLAG(ENABLE_ARCORE)
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
      return std::make_unique<ArCoreInstallHelper>();
#endif  // ENABLE_ARCORE
#endif  // OS_ANDROID
    default:
      return nullptr;
  }
}
}  // namespace vr
