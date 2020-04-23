// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/chrome_xr_integration_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/xr_consent_helper.h"
#include "content/public/browser/xr_install_helper.h"
#include "content/public/common/content_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"

#if defined(OS_WIN)
#include "chrome/browser/vr/consent/win_xr_consent_helper.h"
#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/android/vr/gvr_consent_helper.h"
#include "chrome/browser/android/vr/gvr_install_helper.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/android/vr/arcore_device/arcore_consent_prompt.h"
#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"
#include "device/vr/android/arcore/arcore_device_provider_factory.h"
#endif  // ENABLE_ARCORE
#endif  // OS_WIN/OS_ANDROID

namespace {
bool IsXrDeviceConsentPromptDisabledForTesting() {
  static bool is_xr_device_consent_prompt_disabled_for_testing =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableXrDeviceConsentPromptForTesting);
  return is_xr_device_consent_prompt_disabled_for_testing;
}
}

namespace vr {

// A version of the XrConsentHelper that can be returned if the consent helper
// should be treated as disabled for testing. It will automatically grant
// consent for the requested consent_level. This implementation is provided over
// just returning nullptr because if the product code does not get an object,
// then it assumes that consent was denied.
class AutoGrantingXrConsentHelperForTesting : public content::XrConsentHelper {
  void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      content::XrConsentPromptLevel consent_level,
      content::OnXrUserConsentCallback response_callback) override {
    std::move(response_callback).Run(consent_level, true);
  }
};

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

std::unique_ptr<content::XrConsentHelper>
ChromeXrIntegrationClient::GetConsentHelper(
    device::mojom::XRDeviceId device_id) {
  if (IsXrDeviceConsentPromptDisabledForTesting())
    return std::make_unique<AutoGrantingXrConsentHelperForTesting>();

#if defined(OS_WIN)
  return std::make_unique<WinXrConsentHelper>();
#else
  switch (device_id) {
#if defined(OS_ANDROID)
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
      return std::make_unique<GvrConsentHelper>();
#if BUILDFLAG(ENABLE_ARCORE)
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
      return std::make_unique<ArCoreConsentPrompt>();
#endif  // ENABLE_ARCORE
#endif  // OS_ANDROID
    default:
      return nullptr;
  }
#endif
}

content::XRProviderList ChromeXrIntegrationClient::GetAdditionalProviders() {
  content::XRProviderList providers;

#if defined(OS_ANDROID)
  providers.push_back(std::make_unique<device::GvrDeviceProvider>());
#if BUILDFLAG(ENABLE_ARCORE)
  // TODO(https://crbug.com/966647) remove this check.
  if (base::FeatureList::IsEnabled(features::kWebXrArModule)) {
    auto arcore_device_provider = device::ArCoreDeviceProviderFactory::Create();
    if (arcore_device_provider) {
      providers.push_back(std::move(arcore_device_provider));
    } else {
      // TODO(https://crbug.com/1050470): Remove this logging after
      // investigation.
      LOG(ERROR) << "Could not get ARCoreDeviceProviderFactory";
      base::RecordAction(base::UserMetricsAction(
          "XR.ARCoreDeviceProviderFactory.NotInstalled"));
    }
  }
#endif  // BUILDFLAG(ENABLE_ARCORE)
#endif  // defined(OS_ANDROID)

  return providers;
}

#if defined(OS_WIN)
std::unique_ptr<content::VrUiHost> ChromeXrIntegrationClient::CreateVrUiHost(
    device::mojom::XRDeviceId device_id,
    mojo::PendingRemote<device::mojom::XRCompositorHost> compositor) {
  return std::make_unique<VRUiHostImpl>(device_id, std::move(compositor));
}
#endif
}  // namespace vr
