// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_DEVICE_H_
#define DEVICE_VR_OPENXR_OPENXR_DEVICE_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"

namespace device {

class DEVICE_VR_EXPORT OpenXRDevice
    : public VRDeviceBase,
      public mojom::XRSessionController,
      public mojom::IsolatedXRGamepadProviderFactory,
      public mojom::XRCompositorHost {
 public:
  OpenXRDevice();
  ~OpenXRDevice() override;

  static bool IsHardwareAvailable();
  static bool IsApiAvailable();

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  mojom::IsolatedXRGamepadProviderFactoryPtr BindGamepadFactory();
  mojom::XRCompositorHostPtr BindCompositorHost();

 private:
  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  // mojom::IsolatedXRGamepadProviderFactory
  void GetIsolatedXRGamepadProvider(
      mojom::IsolatedXRGamepadProviderRequest provider_request) override;

  // XRCompositorHost
  void CreateImmersiveOverlay(
      mojom::ImmersiveOverlayRequest overlay_request) override;

  mojo::Binding<mojom::XRSessionController> exclusive_controller_binding_;
  mojo::Binding<mojom::IsolatedXRGamepadProviderFactory>
      gamepad_provider_factory_binding_;
  mojo::Binding<mojom::XRCompositorHost> compositor_host_binding_;

  DISALLOW_COPY_AND_ASSIGN(OpenXRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_DEVICE_H_
