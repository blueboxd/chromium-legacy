// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_device.h"

#include "base/bind_helpers.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_render_loop.h"

namespace device {

namespace {

constexpr float kFramebufferScale = 1.0f;
constexpr float kFov = 45.0f;
constexpr uint32_t kDimension = 1024;
constexpr float kInterpupillaryDistance = 0.1f;  // 10cm

// OpenXR doesn't give out display info until you start a session.
// However our mojo interface expects display info right away to support WebVR.
// We create a fake display info to use, then notify the client that the display
// info changed when we get real data.
mojom::VRDisplayInfoPtr CreateFakeVRDisplayInfo(device::mojom::XRDeviceId id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();

  display_info->id = id;
  display_info->display_name = std::string("OpenXR");

  display_info->capabilities = mojom::VRDisplayCapabilities::New();
  display_info->capabilities->has_position = true;
  display_info->capabilities->has_external_display = true;
  display_info->capabilities->can_present = true;

  display_info->webvr_default_framebuffer_scale = kFramebufferScale;
  display_info->webxr_default_framebuffer_scale = kFramebufferScale;

  display_info->left_eye = mojom::VREyeParameters::New();
  display_info->right_eye = mojom::VREyeParameters::New();

  display_info->left_eye->field_of_view =
      mojom::VRFieldOfView::New(kFov, kFov, kFov, kFov);
  display_info->right_eye->field_of_view =
      display_info->left_eye->field_of_view->Clone();

  display_info->left_eye->render_width = kDimension;
  display_info->left_eye->render_height = kDimension;
  display_info->right_eye->render_width = kDimension;
  display_info->right_eye->render_width = kDimension;

  display_info->left_eye->offset = {-kInterpupillaryDistance * 0.5, 0, 0};
  display_info->right_eye->offset = {kInterpupillaryDistance * 0.5, 0, 0};

  return display_info;
}

}  // namespace

bool OpenXrDevice::IsHardwareAvailable() {
  return OpenXrApiWrapper::IsHardwareAvailable();
}

bool OpenXrDevice::IsApiAvailable() {
  return OpenXrApiWrapper::IsApiAvailable();
}

OpenXrDevice::OpenXrDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      exclusive_controller_binding_(this),
      gamepad_provider_factory_binding_(this),
      compositor_host_binding_(this),
      weak_ptr_factory_(this) {
  render_loop_ = std::make_unique<OpenXrRenderLoop>();
  SetVRDisplayInfo(CreateFakeVRDisplayInfo(GetId()));
}

OpenXrDevice::~OpenXrDevice() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the render loop doesn't get shutdown while it is processing
  // any requests.
  if (render_loop_->IsRunning()) {
    render_loop_->Stop();
  }
}

mojom::IsolatedXRGamepadProviderFactoryPtr OpenXrDevice::BindGamepadFactory() {
  mojom::IsolatedXRGamepadProviderFactoryPtr ret;
  gamepad_provider_factory_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

mojom::XRCompositorHostPtr OpenXrDevice::BindCompositorHost() {
  mojom::XRCompositorHostPtr ret;
  compositor_host_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

void OpenXrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DCHECK(options->immersive);

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr, nullptr);
      return;
    }

    if (provider_request_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestGamepadProvider,
                                    base::Unretained(render_loop_.get()),
                                    std::move(provider_request_)));
    }

    if (overlay_request_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_request_)));
    }
  }

  auto my_callback =
      base::BindOnce(&OpenXrDevice::OnRequestSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  // OpenXR doesn't need to handle anything when presentation has ended, but
  // the mojo interface to call to XRCompositorCommon::RequestSession requires
  // a method and cannot take nullptr, so passing in base::DoNothing::Once()
  // for on_presentation_ended
  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                base::DoNothing::Once(), std::move(options),
                                std::move(my_callback)));
}

void OpenXrDevice::OnRequestSessionResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    bool result,
    mojom::XRSessionPtr session) {
  if (!result) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  OnStartPresenting();

  mojom::XRSessionControllerPtr session_controller;
  exclusive_controller_binding_.Bind(mojo::MakeRequest(&session_controller));

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_binding_.set_connection_error_handler(
      base::BindOnce(&OpenXrDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));

  uint32_t width, height;
  render_loop_->GetViewSize(&width, &height);
  display_info_->left_eye->render_width = width;
  display_info_->right_eye->render_width = width;
  display_info_->left_eye->render_height = height;
  display_info_->right_eye->render_height = height;
  session->display_info = display_info_.Clone();

  std::move(callback).Run(std::move(session), std::move(session_controller));
}

void OpenXrDevice::OnPresentingControllerMojoConnectionError() {
  // This method is called when the rendering process exit presents.
  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::ExitPresent,
                                base::Unretained(render_loop_.get())));
  OnExitPresent();
  exclusive_controller_binding_.Close();
}

void OpenXrDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  NOTREACHED();
}

void OpenXrDevice::GetIsolatedXRGamepadProvider(
    mojom::IsolatedXRGamepadProviderRequest provider_request) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestGamepadProvider,
                                  base::Unretained(render_loop_.get()),
                                  std::move(provider_request)));
  } else {
    provider_request_ = std::move(provider_request);
  }
}

void OpenXrDevice::CreateImmersiveOverlay(
    mojom::ImmersiveOverlayRequest overlay_request) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_request)));
  } else {
    overlay_request_ = std::move(overlay_request);
  }
}

}  // namespace device
