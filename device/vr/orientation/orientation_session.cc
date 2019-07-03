// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/orientation/orientation_session.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/orientation/orientation_device.h"

namespace device {

VROrientationSession::VROrientationSession(
    VROrientationDevice* device,
    mojom::XRFrameDataProviderRequest magic_window_request,
    mojom::XRSessionControllerRequest session_request)
    : magic_window_binding_(this, std::move(magic_window_request)),
      session_controller_binding_(this, std::move(session_request)),
      device_(device) {
  // Unretained is safe because the binding will close when we are destroyed,
  // so we won't receive any more callbacks after that.
  session_controller_binding_.set_connection_error_handler(base::BindOnce(
      &VROrientationSession::OnMojoConnectionError, base::Unretained(this)));
}

VROrientationSession::~VROrientationSession() = default;

// Gets frame data for sessions.
void VROrientationSession::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  // Orientation sessions should never be exclusive or presenting.
  DCHECK(!device_->HasExclusiveSession());

  if (restrict_frame_data_) {
    std::move(callback).Run(nullptr);
    return;
  }

  device_->GetInlineFrameData(std::move(callback));
}

void VROrientationSession::GetEnvironmentIntegrationProvider(
    mojom::XREnvironmentIntegrationProviderAssociatedRequest
        environment_request) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
}

void VROrientationSession::SetInputSourceButtonListener(
    device::mojom::XRInputSourceButtonListenerAssociatedPtrInfo) {
  // Input eventing is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Input eventing is not supported.");
}

// XRSessionController
void VROrientationSession::SetFrameDataRestricted(bool frame_data_restricted) {
  restrict_frame_data_ = frame_data_restricted;
}

void VROrientationSession::OnMojoConnectionError() {
  magic_window_binding_.Close();
  session_controller_binding_.Close();
  device_->EndMagicWindowSession(this);  // This call will destroy us.
}

}  // namespace device
