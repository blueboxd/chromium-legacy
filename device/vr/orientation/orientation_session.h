// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ORIENTATION_ORIENTATION_SESSION_H_
#define DEVICE_VR_ORIENTATION_ORIENTATION_SESSION_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"

namespace device {

class VROrientationDevice;

// VR device process implementation of a XRFrameDataProvider within a session
// that exposes device orientation sensors.
// VROrientationSession objects are owned by their respective
// VROrientationDevice instances.
class DEVICE_VR_EXPORT VROrientationSession
    : public mojom::XRFrameDataProvider,
      public mojom::XRSessionController {
 public:
  VROrientationSession(VROrientationDevice* device,
                       mojom::XRFrameDataProviderRequest,
                       mojom::XRSessionControllerRequest);
  ~VROrientationSession() override;

  void GetEnvironmentIntegrationProvider(
      mojom::XREnvironmentIntegrationProviderAssociatedRequest
          environment_provider) override;
  void SetInputSourceButtonListener(
      mojom::XRInputSourceButtonListenerAssociatedPtrInfo) override;

  // Accessible to tests.
 protected:
  // mojom::XRFrameDataProvider
  void GetFrameData(mojom::XRFrameDataRequestOptionsPtr options,
                    GetFrameDataCallback callback) override;

  // mojom::XRSessionController
  void SetFrameDataRestricted(bool frame_data_restricted) override;

  void OnMojoConnectionError();

  mojo::Binding<mojom::XRFrameDataProvider> magic_window_binding_;
  mojo::Binding<mojom::XRSessionController> session_controller_binding_;
  device::VROrientationDevice* device_;
  bool restrict_frame_data_ = true;
};

}  // namespace device

#endif  //  DEVICE_VR_ORIENTATION_ORIENTATION_SESSION_H_
