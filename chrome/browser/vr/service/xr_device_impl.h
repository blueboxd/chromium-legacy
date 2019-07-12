// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_DEVICE_IMPL_H_
#define CHROME_BROWSER_VR_SERVICE_XR_DEVICE_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"

#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/service/interface_set.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace vr {

class XRRuntimeManager;
class BrowserXRRuntime;

// The browser-side host for VR services. Controls access to VR
// APIs like poses and presentation.
class XRDeviceImpl : public device::mojom::XRDevice {
 public:
  static bool IsXrDeviceConsentPromptDisabledForTesting();

  XRDeviceImpl(content::RenderFrameHost* render_frame_host,
               device::mojom::XRDeviceRequest device_request,
               scoped_refptr<XRRuntimeManager> runtime_manager);
  ~XRDeviceImpl() override;

  // device::mojom::XRDevice
  void RequestSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::XRDevice::RequestSessionCallback callback) override;
  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::XRDevice::SupportsSessionCallback callback) override;
  void ExitPresent() override;
  // device::mojom::XRDevice WebVR compatibility functions
  void GetImmersiveVRDisplayInfo(
      device::mojom::XRDevice::GetImmersiveVRDisplayInfoCallback callback)
      override;

  void SetListeningForActivate(device::mojom::VRDisplayClientPtr client);

  void SetInFocusedFrame(bool in_focused_frame);

  // Notifications/calls from BrowserXRRuntime:
  void RuntimesChanged();
  void OnExitPresent();
  void OnBlur();
  void OnFocus();
  void OnActivate(device::mojom::VRDisplayEventReason reason,
                  base::OnceCallback<void(bool)> on_handled);
  void OnDeactivate(device::mojom::VRDisplayEventReason reason);
  bool ListeningForActivate() { return !!client_; }
  bool InFocusedFrame() { return in_focused_frame_; }

  base::WeakPtr<XRDeviceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::WebContents* GetWebContents();

 private:
  bool IsAnotherHostPresenting();

  // Returns currently active instance of SessionMetricsHelper from WebContents.
  // If the instance is not present on WebContents, it will be created with the
  // assumption that we are not already in VR.
  SessionMetricsHelper* GetSessionMetricsHelper();

  bool InternalSupportsSession(device::mojom::XRSessionOptions* options);
  void OnInlineSessionCreated(
      device::mojom::XRDeviceId session_runtime_id,
      device::mojom::XRDevice::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      device::mojom::XRSessionControllerPtr controller);
  // Called when inline session gets disconnected. |session_id| is the value
  // returned by |magic_window_controllers_| when adding session controller to
  // it.
  void OnInlineSessionDisconnected(size_t session_id);

  void OnSessionCreated(
      device::mojom::XRDeviceId session_runtime_id,
      device::mojom::XRDevice::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session);
  void DoRequestSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::XRDevice::RequestSessionCallback callback);
  void OnConsentResult(device::mojom::XRSessionOptionsPtr options,
                       device::mojom::XRDevice::RequestSessionCallback callback,
                       bool is_consent_granted);

  // TODO(https://crbug.com/837538): Instead, check before returning this
  // object.
  bool IsSecureContextRequirementSatisfied();

  bool in_focused_frame_ = false;

  scoped_refptr<XRRuntimeManager> runtime_manager_;

  content::RenderFrameHost* render_frame_host_;
  mojo::Binding<device::mojom::XRDevice> binding_;
  mojo::InterfacePtrSet<device::mojom::XRSessionClient> session_clients_;
  // This is required for WebVR 1.1 backwards compatibility.
  device::mojom::VRDisplayClientPtr client_;

  InterfaceSet<device::mojom::XRSessionControllerPtr> magic_window_controllers_;

  base::WeakPtrFactory<XRDeviceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(XRDeviceImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_DEVICE_IMPL_H_
