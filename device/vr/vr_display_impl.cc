// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_display_impl.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device_base.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(
    VRDeviceBase* device,
    mojom::XRFrameDataProviderRequest magic_window_request,
    mojom::XRSessionControllerRequest session_request)
    : magic_window_binding_(this, std::move(magic_window_request)),
      session_controller_binding_(this, std::move(session_request)),
      device_(device) {
  // Unretained is safe because the binding will close when we are destroyed,
  // so we won't receive any more callbacks after that.
  session_controller_binding_.set_connection_error_handler(base::BindOnce(
      &VRDisplayImpl::OnMojoConnectionError, base::Unretained(this)));
}

VRDisplayImpl::~VRDisplayImpl() = default;

// Gets frame data for sessions.
void VRDisplayImpl::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  if (device_->HasExclusiveSession() || restrict_frame_data_) {
    std::move(callback).Run(nullptr);
    return;
  }

  device_->GetInlineFrameData(std::move(callback));
}

void VRDisplayImpl::GetEnvironmentIntegrationProvider(
    mojom::XREnvironmentIntegrationProviderAssociatedRequest
        environment_request) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
}

void VRDisplayImpl::SetInputSourceButtonListener(
    device::mojom::XRInputSourceButtonListenerAssociatedPtrInfo) {
  // Input eventing is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Input eventing is not supported.");
}

// XRSessionController
void VRDisplayImpl::SetFrameDataRestricted(bool frame_data_restricted) {
  restrict_frame_data_ = frame_data_restricted;
  if (device_->ShouldPauseTrackingWhenFrameDataRestricted()) {
    if (restrict_frame_data_) {
      device_->PauseTracking();
    } else {
      device_->ResumeTracking();
    }
  }
}

void VRDisplayImpl::OnMojoConnectionError() {
  magic_window_binding_.Close();
  session_controller_binding_.Close();
  device_->EndMagicWindowSession(this);  // This call will destroy us.
}

}  // namespace device
