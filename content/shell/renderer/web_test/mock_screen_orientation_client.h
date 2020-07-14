// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_MOCK_SCREEN_ORIENTATION_CLIENT_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_MOCK_SCREEN_ORIENTATION_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "third_party/blink/public/mojom/widget/screen_orientation.mojom.h"

namespace blink {
class WebLocalFrame;
}

namespace content {

class MockScreenOrientationClient : public device::mojom::ScreenOrientation {
 public:
  explicit MockScreenOrientationClient();
  ~MockScreenOrientationClient() override;

  void ResetData();
  bool UpdateDeviceOrientation(blink::WebLocalFrame* main_frame,
                               blink::mojom::ScreenOrientation orientation);

  blink::mojom::ScreenOrientation CurrentOrientationType() const;
  unsigned CurrentOrientationAngle() const;
  bool IsDisabled() const { return is_disabled_; }
  void SetDisabled(bool disabled);

  void AddReceiver(mojo::ScopedInterfaceEndpointHandle handle);
  void OverrideAssociatedInterfaceProviderForFrame(blink::WebLocalFrame* frame);

  // device::mojom::ScreenOrientation implementation.
  void LockOrientation(device::mojom::ScreenOrientationLockType orientation,
                       LockOrientationCallback callback) override;
  void UnlockOrientation() override;

 private:
  void UpdateLockSync(device::mojom::ScreenOrientationLockType,
                      LockOrientationCallback callback);
  void ResetLockSync();

  bool UpdateScreenOrientation(blink::mojom::ScreenOrientation);
  bool IsOrientationAllowedByCurrentLock(blink::mojom::ScreenOrientation);
  blink::mojom::ScreenOrientation SuitableOrientationForCurrentLock();
  static unsigned OrientationTypeToAngle(blink::mojom::ScreenOrientation);

  blink::WebLocalFrame* main_frame_ = nullptr;
  device::mojom::ScreenOrientationLockType current_lock_ =
      device::mojom::ScreenOrientationLockType::DEFAULT;
  blink::mojom::ScreenOrientation device_orientation_ =
      blink::mojom::ScreenOrientation::kPortraitPrimary;
  blink::mojom::ScreenOrientation current_orientation_ =
      blink::mojom::ScreenOrientation::kPortraitPrimary;
  bool is_disabled_ = false;
  mojo::AssociatedReceiverSet<device::mojom::ScreenOrientation> receivers_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_MOCK_SCREEN_ORIENTATION_CLIENT_H_
