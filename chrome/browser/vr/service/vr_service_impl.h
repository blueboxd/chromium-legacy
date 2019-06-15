// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_
#define CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"

#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace vr {

class XRDeviceImpl;
class XRRuntimeManager;
class XRRuntimeManagerTest;

// Browser process implementation of the VRService mojo interface. Instantiated
// through Mojo once the user loads a page containing WebXR.
class VR_EXPORT VRServiceImpl : public device::mojom::VRService,
                                content::WebContentsObserver {
 public:
  friend XRRuntimeManagerTest;

  explicit VRServiceImpl(content::RenderFrameHost* render_frame_host);
  ~VRServiceImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     device::mojom::VRServiceRequest request);

  // device::mojom::VRService implementation
  void RequestDevice(RequestDeviceCallback callback) override;
  void SetClient(device::mojom::VRServiceClientPtr service_client) override;

  // Tells the renderer that the state of the physical devices changed.
  void RuntimesChanged();

  void InitializationComplete();

 private:
  // Constructor for tests.
  VRServiceImpl();

  // device::mojom::VRService implementation
  void SetListeningForActivate(
      device::mojom::VRDisplayClientPtr client) override;

  // content::WebContentsObserver implementation
  void OnWebContentsFocused(content::RenderWidgetHost* host) override;
  void OnWebContentsLostFocus(content::RenderWidgetHost* host) override;
  void RenderFrameDeleted(content::RenderFrameHost* host) override;

  void OnWebContentsFocusChanged(content::RenderWidgetHost* host, bool focused);

  void MaybeReturnDevice();

  scoped_refptr<XRRuntimeManager> runtime_manager_;
  std::unique_ptr<XRDeviceImpl> device_;
  RequestDeviceCallback request_device_callback_;
  device::mojom::VRServiceClientPtr client_;
  content::RenderFrameHost* render_frame_host_;
  mojo::StrongBindingPtr<VRService> binding_;
  bool initialization_complete_ = false;

  DISALLOW_COPY_AND_ASSIGN(VRServiceImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_SERVICE_IMPL_H_
