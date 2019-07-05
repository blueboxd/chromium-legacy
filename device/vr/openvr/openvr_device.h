// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_OPENVR_DEVICE_H_
#define DEVICE_VR_OPENVR_OPENVR_DEVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/openvr/openvr_api_wrapper.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class XRCompositorCommon;

class DEVICE_VR_EXPORT OpenVRDevice
    : public VRDeviceBase,
      public mojom::XRSessionController,
      public mojom::IsolatedXRGamepadProviderFactory,
      public mojom::XRCompositorHost {
 public:
  OpenVRDevice();
  ~OpenVRDevice() override;

  static bool IsHwAvailable();
  static bool IsApiAvailable();
  static void RecordRuntimeAvailability();

  void Shutdown();

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void EnsureInitialized(EnsureInitializedCallback callback) override;

  void OnPollingEvents();

  void OnRequestSessionResult(mojom::XRRuntime::RequestSessionCallback callback,
                              bool result,
                              mojom::XRSessionPtr session);

  bool IsAvailable();

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

  void OnPresentingControllerMojoConnectionError();
  void OnPresentationEnded();
  bool EnsureValidDisplayInfo();

  int outstanding_session_requests_count_ = 0;
  bool have_real_display_info_ = false;
  std::unique_ptr<XRCompositorCommon> render_loop_;
  std::unique_ptr<OpenVRWrapper> openvr_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  mojo::Binding<mojom::XRSessionController> exclusive_controller_binding_;

  mojo::Binding<mojom::IsolatedXRGamepadProviderFactory>
      gamepad_provider_factory_binding_;
  mojom::IsolatedXRGamepadProviderRequest provider_request_;

  mojo::Binding<mojom::XRCompositorHost> compositor_host_binding_;
  mojom::ImmersiveOverlayRequest overlay_request_;

  base::WeakPtrFactory<OpenVRDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_OPENVR_DEVICE_H_
