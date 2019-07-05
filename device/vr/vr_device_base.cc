// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_base.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "device/vr/vr_device_provider.h"

namespace device {

VRDeviceBase::VRDeviceBase(mojom::XRDeviceId id)
    : id_(id), runtime_binding_(this) {}

VRDeviceBase::~VRDeviceBase() = default;

mojom::XRDeviceId VRDeviceBase::GetId() const {
  return id_;
}

void VRDeviceBase::PauseTracking() {}

void VRDeviceBase::ResumeTracking() {}

mojom::VRDisplayInfoPtr VRDeviceBase::GetVRDisplayInfo() {
  return display_info_.Clone();
}

void VRDeviceBase::OnExitPresent() {
  if (listener_)
    listener_->OnExitPresent();
  presenting_ = false;
}

void VRDeviceBase::OnStartPresenting() {
  presenting_ = true;
}

bool VRDeviceBase::HasExclusiveSession() {
  return presenting_;
}

void VRDeviceBase::ListenToDeviceChanges(
    mojom::XRRuntimeEventListenerAssociatedPtrInfo listener_info,
    mojom::XRRuntime::ListenToDeviceChangesCallback callback) {
  listener_.Bind(std::move(listener_info));
  std::move(callback).Run(display_info_.Clone());
}

void VRDeviceBase::SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  DCHECK(display_info->id == id_);
  display_info_ = std::move(display_info);

  if (listener_)
    listener_->OnDisplayInfoChanged(display_info_.Clone());
}

void VRDeviceBase::OnActivate(mojom::VRDisplayEventReason reason,
                              base::Callback<void(bool)> on_handled) {
  if (listener_)
    listener_->OnDeviceActivated(reason, std::move(on_handled));
}

mojom::XRRuntimePtr VRDeviceBase::BindXRRuntimePtr() {
  mojom::XRRuntimePtr runtime;
  runtime_binding_.Bind(mojo::MakeRequest(&runtime));
  return runtime;
}

void VRDeviceBase::OnListeningForActivate(bool listening) {}

void VRDeviceBase::SetListeningForActivate(bool is_listening) {
  OnListeningForActivate(is_listening);
}

void VRDeviceBase::EnsureInitialized(EnsureInitializedCallback callback) {
  std::move(callback).Run();
}

void VRDeviceBase::SetInlinePosesEnabled(bool enable) {
  inline_poses_enabled_ = enable;
}

void VRDeviceBase::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback) {
  NOTREACHED() << "Unexpected call to a device without hit-test support";
  std::move(callback).Run(base::nullopt);
}

void LogViewerType(VrViewerType type) {
  base::UmaHistogramSparse("VRViewerType", static_cast<int>(type));
}

}  // namespace device
