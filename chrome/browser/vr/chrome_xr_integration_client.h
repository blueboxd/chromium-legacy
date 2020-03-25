// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CHROME_XR_INTEGRATION_CLIENT_H_
#define CHROME_BROWSER_VR_CHROME_XR_INTEGRATION_CLIENT_H_

#include <memory>

#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "content/public/browser/xr_integration_client.h"

class ChromeContentBrowserClient;

namespace vr {

class ChromeXrIntegrationClient : public content::XrIntegrationClient {
 public:
  explicit ChromeXrIntegrationClient(
      util::PassKey<ChromeContentBrowserClient>) {}
  ~ChromeXrIntegrationClient() override = default;
  ChromeXrIntegrationClient(const ChromeXrIntegrationClient&) = delete;
  ChromeXrIntegrationClient& operator=(const ChromeXrIntegrationClient&) =
      delete;

  // XrIntegrationClient
  std::unique_ptr<content::XrInstallHelper> GetInstallHelper(
      device::mojom::XRDeviceId device_id) override;
  std::unique_ptr<content::XrConsentHelper> GetConsentHelper(
      device::mojom::XRDeviceId device_id) override;

  // The only class that we have which implements VrUiHost is Win-only.
#if defined(OS_WIN)
  std::unique_ptr<content::VrUiHost> CreateVrUiHost(
      device::mojom::XRDeviceId device_id,
      mojo::PendingRemote<device::mojom::XRCompositorHost> compositor) override;
#endif

  // TODO(1031622): Once all consumers have been moved to content/, this should
  // be removed, and those consumers should be updated to get this instance from
  // the ContentBrowserClient.
  // Nothing in this class should rely on this singleton behavior.
  // This is simply created as a "Singleton" here to ensure that callers don't
  // just create an instance of this and cache the value.
  static ChromeXrIntegrationClient* GetInstance();

 private:
  ChromeXrIntegrationClient() = default;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CHROME_XR_INTEGRATION_CLIENT_H_
