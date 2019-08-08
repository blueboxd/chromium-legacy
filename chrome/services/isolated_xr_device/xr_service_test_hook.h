// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_SERVICE_TEST_HOOK_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_SERVICE_TEST_HOOK_H_

#include <memory>

#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"

namespace device {
class XRTestHookWrapper;

class XRServiceTestHook : public device_test::mojom::XRServiceTestHook {
 public:
  XRServiceTestHook();
  ~XRServiceTestHook() final;

  using DeviceCrashCallback = device_test::mojom::XRServiceTestHook::
      TerminateDeviceServiceProcessForTestingCallback;
  // device_test::mojom::XRServiceTestHook
  void SetTestHook(device_test::mojom::XRTestHookPtr hook,
                   device_test::mojom::XRServiceTestHook::SetTestHookCallback
                       callback) override;
  void TerminateDeviceServiceProcessForTesting(
      DeviceCrashCallback callback) override;

 private:
  std::unique_ptr<XRTestHookWrapper> wrapper_;
};

}  // namespace device

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_SERVICE_TEST_HOOK_H_
