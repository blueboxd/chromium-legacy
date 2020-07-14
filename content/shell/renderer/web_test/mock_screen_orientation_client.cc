// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/mock_screen_orientation_client.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

MockScreenOrientationClient::MockScreenOrientationClient() = default;

MockScreenOrientationClient::~MockScreenOrientationClient() = default;

void MockScreenOrientationClient::ResetData() {
  main_frame_ = nullptr;
  current_lock_ = device::mojom::ScreenOrientationLockType::DEFAULT;
  device_orientation_ = blink::mojom::ScreenOrientation::kPortraitPrimary;
  current_orientation_ = blink::mojom::ScreenOrientation::kPortraitPrimary;
  is_disabled_ = false;
  receivers_.Clear();
}

bool MockScreenOrientationClient::UpdateDeviceOrientation(
    blink::WebLocalFrame* main_frame,
    blink::mojom::ScreenOrientation orientation) {
  main_frame_ = main_frame;

  if (device_orientation_ == orientation)
    return false;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return false;
  return UpdateScreenOrientation(orientation);
}

bool MockScreenOrientationClient::UpdateScreenOrientation(
    blink::mojom::ScreenOrientation orientation) {
  if (current_orientation_ == orientation)
    return false;
  current_orientation_ = orientation;
  if (main_frame_) {
    main_frame_->SendOrientationChangeEvent();
    return true;
  }
  return false;
}

blink::mojom::ScreenOrientation
MockScreenOrientationClient::CurrentOrientationType() const {
  return current_orientation_;
}

unsigned MockScreenOrientationClient::CurrentOrientationAngle() const {
  return OrientationTypeToAngle(current_orientation_);
}

void MockScreenOrientationClient::SetDisabled(bool disabled) {
  is_disabled_ = disabled;
}

unsigned MockScreenOrientationClient::OrientationTypeToAngle(
    blink::mojom::ScreenOrientation type) {
  unsigned angle;
  // FIXME(ostap): This relationship between orientationType and
  // orientationAngle is temporary. The test should be able to specify
  // the angle in addition to the orientation type.
  switch (type) {
    case blink::mojom::ScreenOrientation::kLandscapePrimary:
      angle = 90;
      break;
    case blink::mojom::ScreenOrientation::kLandscapeSecondary:
      angle = 270;
      break;
    case blink::mojom::ScreenOrientation::kPortraitSecondary:
      angle = 180;
      break;
    default:
      angle = 0;
  }
  return angle;
}

bool MockScreenOrientationClient::IsOrientationAllowedByCurrentLock(
    blink::mojom::ScreenOrientation orientation) {
  if (current_lock_ == device::mojom::ScreenOrientationLockType::DEFAULT ||
      current_lock_ == device::mojom::ScreenOrientationLockType::ANY) {
    return true;
  }

  switch (orientation) {
    case blink::mojom::ScreenOrientation::kPortraitPrimary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT;
    case blink::mojom::ScreenOrientation::kPortraitSecondary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::PORTRAIT;
    case blink::mojom::ScreenOrientation::kLandscapePrimary:
      return current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE;
    case blink::mojom::ScreenOrientation::kLandscapeSecondary:
      return current_lock_ == device::mojom::ScreenOrientationLockType::
                                  LANDSCAPE_SECONDARY ||
             current_lock_ ==
                 device::mojom::ScreenOrientationLockType::LANDSCAPE;
    default:
      return false;
  }
}

void MockScreenOrientationClient::AddReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receivers_.Add(
      this, mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>(
                std::move(handle)));
}

void MockScreenOrientationClient::OverrideAssociatedInterfaceProviderForFrame(
    blink::WebLocalFrame* frame) {
  if (!frame)
    return;

  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  blink::AssociatedInterfaceProvider* provider =
      render_frame->GetRemoteAssociatedInterfaces();

  provider->OverrideBinderForTesting(
      device::mojom::ScreenOrientation::Name_,
      base::BindRepeating(&MockScreenOrientationClient::AddReceiver,
                          base::Unretained(this)));
}

void MockScreenOrientationClient::LockOrientation(
    device::mojom::ScreenOrientationLockType orientation,
    LockOrientationCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockScreenOrientationClient::UpdateLockSync,
                     base::Unretained(this), orientation, std::move(callback)));
}

void MockScreenOrientationClient::UnlockOrientation() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockScreenOrientationClient::ResetLockSync,
                                base::Unretained(this)));
}

void MockScreenOrientationClient::UpdateLockSync(
    device::mojom::ScreenOrientationLockType lock,
    LockOrientationCallback callback) {
  DCHECK(lock != device::mojom::ScreenOrientationLockType::DEFAULT);
  current_lock_ = lock;
  if (!IsOrientationAllowedByCurrentLock(current_orientation_))
    UpdateScreenOrientation(SuitableOrientationForCurrentLock());
  std::move(callback).Run(device::mojom::ScreenOrientationLockResult::
                              SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
}

void MockScreenOrientationClient::ResetLockSync() {
  bool will_screen_orientation_need_updating =
      !IsOrientationAllowedByCurrentLock(device_orientation_);
  current_lock_ = device::mojom::ScreenOrientationLockType::DEFAULT;
  if (will_screen_orientation_need_updating)
    UpdateScreenOrientation(device_orientation_);
}

blink::mojom::ScreenOrientation
MockScreenOrientationClient::SuitableOrientationForCurrentLock() {
  switch (current_lock_) {
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return blink::mojom::ScreenOrientation::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return blink::mojom::ScreenOrientation::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return blink::mojom::ScreenOrientation::kLandscapePrimary;
    default:
      return blink::mojom::ScreenOrientation::kPortraitPrimary;
  }
}

}  // namespace content
