// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_APPLICATION_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_APPLICATION_BRIDGE_H_

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/alert.mojom.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace remote_cocoa {

// This class implements the remote_cocoa::mojom::Application interface, and
// bridges that C++ interface to the Objective-C NSApplication. This class is
// the root of all other remote_cocoa classes (e.g, NSAlerts, NSWindows,
// NSViews).
class REMOTE_COCOA_APP_SHIM_EXPORT ApplicationBridge
    : public mojom::Application {
 public:
  static ApplicationBridge* Get();
  void BindRequest(mojom::ApplicationAssociatedRequest request);

  // Set callbacks to create content types (content types cannot be created
  // in remote_cocoa).
  // TODO(https://crbug.com/888290): Move these types from content to
  // remote_cocoa.
  using RenderWidgetHostNSViewCreateCallback = base::RepeatingCallback<void(
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle)>;
  using WebContentsNSViewCreateCallback = base::RepeatingCallback<void(
      uint64_t view_id,
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle)>;
  void SetContentNSViewCreateCallbacks(
      RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback,
      WebContentsNSViewCreateCallback web_conents_create_callback);

  // mojom::Application:
  void CreateAlert(mojom::AlertBridgeRequest bridge_request) override;
  void ShowColorPanel(mojom::ColorPanelRequest request,
                      mojom::ColorPanelHostPtr host) override;
  void CreateNativeWidgetNSWindow(
      uint64_t bridge_id,
      mojom::NativeWidgetNSWindowAssociatedRequest bridge_request,
      mojom::NativeWidgetNSWindowHostAssociatedPtrInfo host,
      mojom::TextInputHostAssociatedPtrInfo text_input_host) override;
  void CreateRenderWidgetHostNSView(
      mojom::StubInterfaceAssociatedPtrInfo host,
      mojom::StubInterfaceAssociatedRequest view_request) override;
  void CreateWebContentsNSView(
      uint64_t view_id,
      mojom::StubInterfaceAssociatedPtrInfo host,
      mojom::StubInterfaceAssociatedRequest view_request) override;

 private:
  friend class base::NoDestructor<ApplicationBridge>;
  ApplicationBridge();
  ~ApplicationBridge() override;

  RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback_;
  WebContentsNSViewCreateCallback web_conents_create_callback_;

  mojo::AssociatedBinding<mojom::Application> binding_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_APPLICATION_BRIDGE_H_
