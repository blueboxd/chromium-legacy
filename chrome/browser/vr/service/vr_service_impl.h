// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_
#define CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/util/type_safety/pass_key.h"

#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/service/interface_set.h"
#include "chrome/browser/vr/service/xr_consent_prompt_level.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace vr {

class XRRuntimeManager;
class XRRuntimeManagerTest;
class BrowserXRRuntime;

// Browser process implementation of the VRService mojo interface. Instantiated
// through Mojo once the user loads a page containing WebXR.
class VR_EXPORT VRServiceImpl : public device::mojom::VRService,
                                content::WebContentsObserver {
 public:
  static bool IsXrDeviceConsentPromptDisabledForTesting();

  explicit VRServiceImpl(content::RenderFrameHost* render_frame_host);

  // Constructor for tests.
  explicit VRServiceImpl(util::PassKey<XRRuntimeManagerTest>);

  ~VRServiceImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     device::mojom::VRServiceRequest request);

  // device::mojom::VRService implementation
  void SetClient(mojo::PendingRemote<device::mojom::VRServiceClient>
                     service_client) override;
  void RequestSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::RequestSessionCallback callback) override;
  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::SupportsSessionCallback callback) override;
  void ExitPresent() override;
  // device::mojom::VRService WebVR compatibility functions
  void GetImmersiveVRDisplayInfo(
      device::mojom::VRService::GetImmersiveVRDisplayInfoCallback callback)
      override;

  void InitializationComplete();

  // Called when inline session gets disconnected. |session_id| is the value
  // returned by |magic_window_controllers_| when adding session controller to
  // it.
  void OnInlineSessionDisconnected(size_t session_id);

  // Notifications/calls from BrowserXRRuntime:
  void OnExitPresent();
  void OnVisibilityStateChanged(
      device::mojom::XRVisibilityState visibility_state);
  void OnActivate(device::mojom::VRDisplayEventReason reason,
                  base::OnceCallback<void(bool)> on_handled);
  void OnDeactivate(device::mojom::VRDisplayEventReason reason);
  bool ListeningForActivate() { return !!display_client_; }
  bool InFocusedFrame() { return in_focused_frame_; }
  void OnDisplayInfoChanged();
  void RuntimesChanged();

  base::WeakPtr<VRServiceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::WebContents* GetWebContents();

  void SetListeningForActivate(
      mojo::PendingRemote<device::mojom::VRDisplayClient> display_client)
      override;

 private:
  // content::WebContentsObserver implementation
  void OnWebContentsFocused(content::RenderWidgetHost* host) override;
  void OnWebContentsLostFocus(content::RenderWidgetHost* host) override;
  void RenderFrameDeleted(content::RenderFrameHost* host) override;

  void OnWebContentsFocusChanged(content::RenderWidgetHost* host, bool focused);

  void ResolvePendingRequests();
  bool IsSecureContextRequirementSatisfied();

  // Returns currently active instance of SessionMetricsHelper from WebContents.
  // If the instance is not present on WebContents, it will be created with the
  // assumption that we are not already in VR.
  SessionMetricsHelper* GetSessionMetricsHelper();

  bool InternalSupportsSession(device::mojom::XRSessionOptions* options);
  void OnInlineSessionCreated(
      device::mojom::XRDeviceId session_runtime_id,
      device::mojom::VRService::RequestSessionCallback callback,
      const std::set<device::mojom::XRSessionFeature>& enabled_features,
      device::mojom::XRSessionPtr session,
      device::mojom::XRSessionControllerPtr controller);

  void OnSessionCreated(
      device::mojom::XRDeviceId session_runtime_id,
      device::mojom::VRService::RequestSessionCallback callback,
      const std::set<device::mojom::XRSessionFeature>& enabled_features,
      device::mojom::XRSessionPtr session);
  void DoRequestSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::RequestSessionCallback callback,
      std::set<device::mojom::XRSessionFeature> enabled_features);
  void ShowConsentPrompt(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::RequestSessionCallback callback,
      const BrowserXRRuntime* runtime,
      std::set<device::mojom::XRSessionFeature> requested_features);
  void OnConsentResult(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::RequestSessionCallback callback,
      std::set<device::mojom::XRSessionFeature> enabled_features,
      XrConsentPromptLevel consent_level,
      bool is_consent_granted);

  bool IsConsentGrantedForDevice(device::mojom::XRDeviceId device_id,
                                 XrConsentPromptLevel consent_level);
  void AddConsentGrantedDevice(device::mojom::XRDeviceId device_id,
                               XrConsentPromptLevel consent_level);

  scoped_refptr<XRRuntimeManager> runtime_manager_;
  mojo::InterfacePtrSet<device::mojom::XRSessionClient> session_clients_;
  mojo::Remote<device::mojom::VRServiceClient> service_client_;
  content::RenderFrameHost* render_frame_host_;
  mojo::StrongBindingPtr<VRService> binding_;
  InterfaceSet<device::mojom::XRSessionControllerPtr> magic_window_controllers_;

  // List of callbacks to run when initialization is completed.
  std::vector<base::OnceCallback<void()>> pending_requests_;

  // This is required for WebVR 1.1 backwards compatibility.
  mojo::Remote<device::mojom::VRDisplayClient> display_client_;

  bool initialization_complete_ = false;
  bool in_focused_frame_ = false;

  std::map<device::mojom::XRDeviceId, XrConsentPromptLevel>
      consent_granted_devices_;

  base::WeakPtrFactory<VRServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VRServiceImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_
