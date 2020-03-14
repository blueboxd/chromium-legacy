// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/xr_integration_client.h"

#include "content/public/browser/xr_install_helper.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"

namespace content {

std::unique_ptr<content::XrInstallHelper> XrIntegrationClient::GetInstallHelper(
    device::mojom::XRDeviceId device_id) {
  return nullptr;
}
}  // namespace content
