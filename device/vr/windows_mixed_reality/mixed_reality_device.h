// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_DEVICE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_DEVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/windows/compositor_base.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class DEVICE_VR_EXPORT MixedRealityDevice
    : public VRDeviceBase,
      public mojom::XRSessionController,
      public mojom::IsolatedXRGamepadProviderFactory,
      public mojom::XRCompositorHost {
 public:
  MixedRealityDevice();
  ~MixedRealityDevice() override;

  mojom::IsolatedXRGamepadProviderFactoryPtr BindGamepadFactory();
  mojom::XRCompositorHostPtr BindCompositorHost();

 private:
  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  // mojom::IsolatedXRGamepadProviderFactory
  void GetIsolatedXRGamepadProvider(
      mojom::IsolatedXRGamepadProviderRequest provider_request) override;

  // XRCompositorHost
  void CreateImmersiveOverlay(
      mojom::ImmersiveOverlayRequest overlay_request) override;

  void CreateRenderLoop();
  void Shutdown();
  void OnPresentingControllerMojoConnectionError();
  void OnPresentationEnded();
  void OnRequestSessionResult(mojom::XRRuntime::RequestSessionCallback callback,
                              bool result,
                              mojom::XRSessionPtr session);

  std::unique_ptr<XRCompositorCommon> render_loop_;

  mojo::Binding<mojom::IsolatedXRGamepadProviderFactory>
      gamepad_provider_factory_binding_;
  mojom::IsolatedXRGamepadProviderRequest provider_request_;

  mojo::Binding<mojom::XRCompositorHost> compositor_host_binding_;
  mojom::ImmersiveOverlayRequest overlay_request_;

  mojo::Binding<mojom::XRSessionController> exclusive_controller_binding_;

  base::WeakPtrFactory<MixedRealityDevice> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MixedRealityDevice);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_DEVICE_H_
