// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/xr_test_utils.h"

#include "chrome/browser/vr/service/xr_device_service.h"

namespace vr {

const mojo::Remote<device::mojom::XRDeviceService>&
GetXRDeviceServiceForTesting() {
  return GetXRDeviceService();
}

void SetXRDeviceServiceStartupCallbackForTesting(
    base::RepeatingClosure callback) {
  SetXRDeviceServiceStartupCallbackForTestingInternal(std::move(callback));
}

}  // namespace vr
