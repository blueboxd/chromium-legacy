// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_XR_INTEGRATION_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_XR_INTEGRATION_CLIENT_H_

#include <memory>

#include "content/common/content_export.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"

namespace content {
class XrInstallHelper;

// A helper class for |ContentBrowserClient| to wrap for XR-specific
// integration that may be needed from content/. Currently it only provides
// access to relevant XrInstallHelpers, but will eventually be expanded to
// include integration points for VrUiHost.
// This should be implemented by embedders.
class CONTENT_EXPORT XrIntegrationClient {
 public:
  XrIntegrationClient() = default;
  virtual ~XrIntegrationClient() = default;
  XrIntegrationClient(const XrIntegrationClient&) = delete;
  XrIntegrationClient& operator=(const XrIntegrationClient&) = delete;

  // Returns the |XrInstallHelper| for the corresponding |XRDeviceId|, or
  // nullptr if the requested |XRDeviceId| does not have any required extra
  // installation steps.
  virtual std::unique_ptr<XrInstallHelper> GetInstallHelper(
      device::mojom::XRDeviceId device_id);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_XR_INTEGRATION_CLIENT_H_
