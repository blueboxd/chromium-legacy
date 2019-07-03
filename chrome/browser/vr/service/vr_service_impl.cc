// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_device_impl.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/vr_device.h"

namespace vr {

VRServiceImpl::VRServiceImpl(content::RenderFrameHost* render_frame_host)
    : WebContentsObserver(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_);

  runtime_manager_ = XRRuntimeManager::GetOrCreateInstance();
  runtime_manager_->AddService(this);
}

// Constructor for testing.
VRServiceImpl::VRServiceImpl() : render_frame_host_(nullptr) {
  runtime_manager_ = XRRuntimeManager::GetOrCreateInstance();
  runtime_manager_->AddService(this);
}

VRServiceImpl::~VRServiceImpl() {
  device_ = nullptr;
  runtime_manager_->RemoveService(this);
}

void VRServiceImpl::Create(content::RenderFrameHost* render_frame_host,
                           device::mojom::VRServiceRequest request) {
  std::unique_ptr<VRServiceImpl> vr_service_impl =
      std::make_unique<VRServiceImpl>(render_frame_host);

  VRServiceImpl* impl = vr_service_impl.get();
  impl->binding_ =
      mojo::MakeStrongBinding(std::move(vr_service_impl), std::move(request));
}

void VRServiceImpl::InitializationComplete() {
  // device_ is returned after all providers have initialized. This means that
  // we can correctly answer SupportsSession, and can provide correct display
  // capabilities.
  initialization_complete_ = true;
  MaybeReturnDevice();
}

void VRServiceImpl::RequestDevice(RequestDeviceCallback callback) {
  if (request_device_callback_) {
    // There should only be one pending VRService::RequestDevice at a time.
    // If request device is called multiple times on the renderer side, the
    // requests should be queued there and returned when this single call
    // returns.
    mojo::ReportBadMessage(
        "Request device called before previous call completed.");
  }

  request_device_callback_ = std::move(callback);
  MaybeReturnDevice();
}

void VRServiceImpl::SetClient(device::mojom::VRServiceClientPtr client) {
  client_ = std::move(client);
}

void VRServiceImpl::MaybeReturnDevice() {
  if (request_device_callback_ && initialization_complete_) {
    // If we are requesting a new device, destroy the old one and create a new
    // one. We assume that the renderer will use the old device until it has
    // been destroyed, so it is safe to destroy it on the browser side.
    device::mojom::XRDevicePtr device;
    if (runtime_manager_->HasAnyRuntime()) {
      device_ = std::make_unique<XRDeviceImpl>(
          render_frame_host_, mojo::MakeRequest(&device), runtime_manager_);
    }
    std::move(request_device_callback_).Run(std::move(device));
  }
}

void VRServiceImpl::RuntimesChanged() {
  if (device_) {
    device_->RuntimesChanged();
  }

  if (client_) {
    client_->OnDeviceChanged();
  }
}

void VRServiceImpl::SetListeningForActivate(
    device::mojom::VRDisplayClientPtr client) {
  if (device_)
    device_->SetListeningForActivate(std::move(client));
}

void VRServiceImpl::OnWebContentsFocused(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, true);
}

void VRServiceImpl::OnWebContentsLostFocus(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, false);
}

void VRServiceImpl::RenderFrameDeleted(content::RenderFrameHost* host) {
  if (host != render_frame_host_)
    return;

  // Binding should always be live here, as this is a StrongBinding.
  // Close the binding (and delete this VrServiceImpl) when the RenderFrameHost
  // is deleted.
  DCHECK(binding_.get());
  binding_->Close();
}

void VRServiceImpl::OnWebContentsFocusChanged(content::RenderWidgetHost* host,
                                              bool focused) {
  if (!render_frame_host_->GetView() ||
      render_frame_host_->GetView()->GetRenderWidgetHost() != host) {
    return;
  }
  if (device_)
    device_->SetInFocusedFrame(focused);
}

}  // namespace vr
