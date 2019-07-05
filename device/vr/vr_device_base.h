// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_BASE_H_
#define DEVICE_VR_VR_DEVICE_BASE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"

namespace device {

// Represents one of the platform's VR devices. Owned by the respective
// VRDeviceProvider.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDeviceBase : public mojom::XRRuntime {
 public:
  explicit VRDeviceBase(mojom::XRDeviceId id);
  ~VRDeviceBase() override;

  // VRDevice Implementation
  void ListenToDeviceChanges(
      mojom::XRRuntimeEventListenerAssociatedPtrInfo listener,
      mojom::XRRuntime::ListenToDeviceChangesCallback callback) final;
  void SetListeningForActivate(bool is_listening) override;
  void EnsureInitialized(EnsureInitializedCallback callback) override;
  void SetInlinePosesEnabled(bool enable) override;

  virtual void RequestHitTest(
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback);
  device::mojom::XRDeviceId GetId() const;

  bool HasExclusiveSession();

  // Devices may be paused/resumed when focus changes by GVR delegate.
  virtual void PauseTracking();
  virtual void ResumeTracking();

  mojom::VRDisplayInfoPtr GetVRDisplayInfo();

  // Used by providers to bind devices.
  mojom::XRRuntimePtr BindXRRuntimePtr();

  // TODO(mthiesse): The browser should handle browser-side exiting of
  // presentation before device/ is even aware presentation is being exited.
  // Then the browser should call StopSession() on Device, which does device/
  // exiting of presentation before notifying displays. This is currently messy
  // because browser-side notions of presentation are mostly Android-specific.
  virtual void OnExitPresent();

 protected:
  // Devices tell VRDeviceBase when they start presenting.  It will be paired
  // with an OnExitPresent when the device stops presenting.
  void OnStartPresenting();
  bool IsPresenting() { return presenting_; }  // Exposed for test.
  void SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info);
  void OnActivate(mojom::VRDisplayEventReason reason,
                  base::Callback<void(bool)> on_handled);

  mojom::VRDisplayInfoPtr display_info_;

  bool inline_poses_enabled_ = true;

 private:
  // TODO(https://crbug.com/842227): Rename methods to HandleOnXXX
  virtual void OnListeningForActivate(bool listening);

  mojom::XRRuntimeEventListenerAssociatedPtr listener_;

  bool presenting_ = false;

  device::mojom::XRDeviceId id_;

  mojo::Binding<mojom::XRRuntime> runtime_binding_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBase);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_BASE_H_
