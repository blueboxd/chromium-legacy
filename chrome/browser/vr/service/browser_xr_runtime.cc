// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/browser_xr_runtime.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/vr/service/xr_device_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/vr_device.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace vr {

bool IsValidStandingTransform(const gfx::Transform& transform) {
  if (!transform.IsInvertible() || transform.HasPerspective())
    return false;

  gfx::DecomposedTransform decomp;
  if (!DecomposeTransform(&decomp, transform))
    return false;

  float kEpsilon = 0.1f;
  float kMaxTranslate = 1000000;  // Maximum 1000km translation.
  if (abs(decomp.perspective[3] - 1) > kEpsilon) {
    // If testing with unexpectedly high values, catch on debug builds rather
    // than silently change data.  On release builds its better to be safe and
    // validate.
    DCHECK(false);
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    if (abs(decomp.scale[i] - 1) > kEpsilon)
      return false;
    if (abs(decomp.skew[i]) > kEpsilon)
      return false;
    if (abs(decomp.perspective[i]) > kEpsilon)
      return false;
    if (abs(decomp.translate[i]) > kMaxTranslate)
      return false;
  }

  // Only rotate and translate.
  return true;
}

device::mojom::VREyeParametersPtr ValidateEyeParameters(
    const device::mojom::VREyeParameters* eye) {
  if (!eye)
    return nullptr;
  device::mojom::VREyeParametersPtr ret = device::mojom::VREyeParameters::New();
  // FOV
  float kDefaultFOV = 45;
  ret->field_of_view = device::mojom::VRFieldOfView::New();
  if (eye->field_of_view->up_degrees < 90 &&
      eye->field_of_view->up_degrees > -90 &&
      eye->field_of_view->up_degrees > -eye->field_of_view->down_degrees &&
      eye->field_of_view->down_degrees < 90 &&
      eye->field_of_view->down_degrees > -90 &&
      eye->field_of_view->down_degrees > -eye->field_of_view->up_degrees &&
      eye->field_of_view->left_degrees < 90 &&
      eye->field_of_view->left_degrees > -90 &&
      eye->field_of_view->left_degrees > -eye->field_of_view->right_degrees &&
      eye->field_of_view->right_degrees < 90 &&
      eye->field_of_view->right_degrees > -90 &&
      eye->field_of_view->right_degrees > -eye->field_of_view->left_degrees) {
    ret->field_of_view->up_degrees = eye->field_of_view->up_degrees;
    ret->field_of_view->down_degrees = eye->field_of_view->down_degrees;
    ret->field_of_view->left_degrees = eye->field_of_view->left_degrees;
    ret->field_of_view->right_degrees = eye->field_of_view->right_degrees;
  } else {
    ret->field_of_view->up_degrees = kDefaultFOV;
    ret->field_of_view->down_degrees = kDefaultFOV;
    ret->field_of_view->left_degrees = kDefaultFOV;
    ret->field_of_view->right_degrees = kDefaultFOV;
  }

  // Offset
  float kMaxOffset = 10;
  if (abs(eye->offset.x()) < kMaxOffset && abs(eye->offset.y()) < kMaxOffset &&
      abs(eye->offset.z()) < kMaxOffset) {
    ret->offset = eye->offset;
  } else {
    ret->offset = gfx::Vector3dF(0, 0, 0);
  }

  // Renderwidth/height
  uint32_t kMaxSize = 16384;
  uint32_t kMinSize = 2;
  // DCHECK on debug builds to catch legitimate large sizes, but clamp on
  // release builds to ensure valid state.
  DCHECK(eye->render_width < kMaxSize);
  DCHECK(eye->render_height < kMaxSize);
  ret->render_width = std::max(std::min(kMaxSize, eye->render_width), kMinSize);
  ret->render_height =
      std::max(std::min(kMaxSize, eye->render_height), kMinSize);
  return ret;
}

device::mojom::VRDisplayInfoPtr ValidateVRDisplayInfo(
    const device::mojom::VRDisplayInfo* info,
    device::mojom::XRDeviceId id) {
  if (!info)
    return nullptr;

  device::mojom::VRDisplayInfoPtr ret = device::mojom::VRDisplayInfo::New();

  // Rather than just cloning everything, we copy over each field and validate
  // individually.  This ensures new fields don't bypass validation.
  ret->id = id;
  ret->display_name = info->display_name;
  DCHECK(info->capabilities);  // Ensured by mojo.
  ret->capabilities = device::mojom::VRDisplayCapabilities::New(
      info->capabilities->has_position,
      info->capabilities->has_external_display, info->capabilities->can_present,
      info->capabilities->can_provide_environment_integration);

  if (info->stage_parameters &&
      IsValidStandingTransform(info->stage_parameters->standing_transform)) {
    ret->stage_parameters = device::mojom::VRStageParameters::New(
        info->stage_parameters->standing_transform,
        info->stage_parameters->size_x, info->stage_parameters->size_z,
        info->stage_parameters->bounds);
  }

  ret->left_eye = ValidateEyeParameters(info->left_eye.get());
  ret->right_eye = ValidateEyeParameters(info->right_eye.get());

  float kMinFramebufferScale = 0.1f;
  float kMaxFramebufferScale = 1.0f;
  if (info->webvr_default_framebuffer_scale <= kMaxFramebufferScale &&
      info->webvr_default_framebuffer_scale >= kMinFramebufferScale) {
    ret->webvr_default_framebuffer_scale =
        info->webvr_default_framebuffer_scale;
  } else {
    ret->webvr_default_framebuffer_scale = 1;
  }

  if (info->webxr_default_framebuffer_scale <= kMaxFramebufferScale &&
      info->webxr_default_framebuffer_scale >= kMinFramebufferScale) {
    ret->webxr_default_framebuffer_scale =
        info->webxr_default_framebuffer_scale;
  } else {
    ret->webxr_default_framebuffer_scale = 1;
  }
  return ret;
}

BrowserXRRuntime::BrowserXRRuntime(device::mojom::XRDeviceId id,
                                   device::mojom::XRRuntimePtr runtime,
                                   device::mojom::VRDisplayInfoPtr display_info)
    : id_(id),
      runtime_(std::move(runtime)),
      display_info_(ValidateVRDisplayInfo(display_info.get(), id)),
      binding_(this) {
  device::mojom::XRRuntimeEventListenerAssociatedPtr listener;
  binding_.Bind(mojo::MakeRequest(&listener));

  // Unretained is safe because we are calling through an InterfacePtr we own,
  // so we won't be called after runtime_ is destroyed.
  runtime_->ListenToDeviceChanges(
      listener.PassInterface(),
      base::BindOnce(&BrowserXRRuntime::OnDisplayInfoChanged,
                     base::Unretained(this)));
}

BrowserXRRuntime::~BrowserXRRuntime() = default;

void BrowserXRRuntime::ExitVrFromPresentingRendererDevice() {
  auto* xr_device = GetPresentingRendererDevice();
  if (xr_device) {
    xr_device->ExitPresent();
  }
}

void BrowserXRRuntime::OnDisplayInfoChanged(
    device::mojom::VRDisplayInfoPtr vr_device_info) {
  bool had_display_info = !!display_info_;
  display_info_ = ValidateVRDisplayInfo(vr_device_info.get(), id_);
  if (had_display_info) {
    for (XRDeviceImpl* device : renderer_device_connections_) {
      device->RuntimesChanged();
    }
  }

  // Notify observers of the new display info.
  for (BrowserXRRuntimeObserver& observer : observers_) {
    observer.SetVRDisplayInfo(display_info_.Clone());
  }
}

void BrowserXRRuntime::StopImmersiveSession() {
  if (immersive_session_controller_) {
    immersive_session_controller_ = nullptr;
    presenting_renderer_device_ = nullptr;

    for (BrowserXRRuntimeObserver& observer : observers_) {
      observer.SetWebXRWebContents(nullptr);
    }
  }
}

void BrowserXRRuntime::OnExitPresent() {
  if (presenting_renderer_device_) {
    presenting_renderer_device_->OnExitPresent();
    presenting_renderer_device_ = nullptr;
  }
}

void BrowserXRRuntime::OnDeviceActivated(
    device::mojom::VRDisplayEventReason reason,
    base::OnceCallback<void(bool)> on_handled) {
  if (listening_for_activation_renderer_device_) {
    listening_for_activation_renderer_device_->OnActivate(
        reason, std::move(on_handled));
  } else {
    std::move(on_handled).Run(true /* will_not_present */);
  }
}

void BrowserXRRuntime::OnDeviceIdle(
    device::mojom::VRDisplayEventReason reason) {
  for (XRDeviceImpl* device : renderer_device_connections_) {
    device->OnDeactivate(reason);
  }
}

void BrowserXRRuntime::OnInitialized() {
  for (auto& callback : pending_initialization_callbacks_) {
    std::move(callback).Run(display_info_.Clone());
  }
  pending_initialization_callbacks_.clear();
}

void BrowserXRRuntime::OnRendererDeviceAdded(XRDeviceImpl* device) {
  renderer_device_connections_.insert(device);
}

void BrowserXRRuntime::OnRendererDeviceRemoved(XRDeviceImpl* device) {
  DCHECK(device);
  renderer_device_connections_.erase(device);
  if (device == presenting_renderer_device_) {
    ExitPresent(device);
    DCHECK(presenting_renderer_device_ == nullptr);
  }
  if (device == listening_for_activation_renderer_device_) {
    // Not listening for activation.
    listening_for_activation_renderer_device_ = nullptr;
    runtime_->SetListeningForActivate(false);
  }
}

void BrowserXRRuntime::ExitPresent(XRDeviceImpl* device) {
  if (device == presenting_renderer_device_) {
    StopImmersiveSession();
  }
}

void BrowserXRRuntime::RequestSession(
    XRDeviceImpl* device,
    const device::mojom::XRRuntimeSessionOptionsPtr& options,
    RequestSessionCallback callback) {
  // base::Unretained is safe because we won't be called back after runtime_ is
  // destroyed.
  runtime_->RequestSession(
      options->Clone(),
      base::BindOnce(&BrowserXRRuntime::OnRequestSessionResult,
                     base::Unretained(this), device->GetWeakPtr(),
                     options->Clone(), std::move(callback)));
}

void BrowserXRRuntime::OnRequestSessionResult(
    base::WeakPtr<XRDeviceImpl> device,
    device::mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback,
    device::mojom::XRSessionPtr session,
    device::mojom::XRSessionControllerPtr immersive_session_controller) {
  if (session && device) {
    if (options->immersive) {
      presenting_renderer_device_ = device.get();
      immersive_session_controller_ = std::move(immersive_session_controller);
      immersive_session_controller_.set_connection_error_handler(base::BindOnce(
          &BrowserXRRuntime::OnImmersiveSessionError, base::Unretained(this)));

      // Notify observers that we have started presentation.
      content::WebContents* web_contents = device->GetWebContents();
      for (BrowserXRRuntimeObserver& observer : observers_) {
        observer.SetWebXRWebContents(web_contents);
      }
    }

    std::move(callback).Run(std::move(session));
  } else {
    std::move(callback).Run(nullptr);
    if (session) {
      // The device has been removed, but we still got a session, so make
      // sure to clean up this weird state.
      immersive_session_controller_ = std::move(immersive_session_controller);
      StopImmersiveSession();
    }
  }
}

void BrowserXRRuntime::OnImmersiveSessionError() {
  StopImmersiveSession();
}

void BrowserXRRuntime::UpdateListeningForActivate(XRDeviceImpl* device) {
  if (device->ListeningForActivate() && device->InFocusedFrame()) {
    bool was_listening = !!listening_for_activation_renderer_device_;
    listening_for_activation_renderer_device_ = device;
    if (!was_listening)
      OnListeningForActivate(true);
  } else if (listening_for_activation_renderer_device_ == device) {
    listening_for_activation_renderer_device_ = nullptr;
    OnListeningForActivate(false);
  }
}

void BrowserXRRuntime::InitializeAndGetDisplayInfo(
    content::RenderFrameHost* render_frame_host,
    device::mojom::XRDevice::GetImmersiveVRDisplayInfoCallback callback) {
  device::mojom::VRDisplayInfoPtr device_info = GetVRDisplayInfo();
  if (device_info) {
    std::move(callback).Run(std::move(device_info));
    return;
  }

  pending_initialization_callbacks_.push_back(std::move(callback));
  runtime_->EnsureInitialized(
      base::BindOnce(&BrowserXRRuntime::OnInitialized, base::Unretained(this)));
}

void BrowserXRRuntime::OnListeningForActivate(bool is_listening) {
  runtime_->SetListeningForActivate(is_listening);
}

}  // namespace vr
