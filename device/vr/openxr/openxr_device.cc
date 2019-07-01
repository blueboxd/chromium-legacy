// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_device.h"

#include "base/bind_helpers.h"

namespace device {

namespace {

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

  display_info->webvr_default_framebuffer_scale = 1.0f;
  display_info->webxr_default_framebuffer_scale = 1.0f;

  display_info->left_eye = mojom::VREyeParameters::New();
  display_info->right_eye = mojom::VREyeParameters::New();

  constexpr float kFov = 45.0f;
  display_info->left_eye->field_of_view =
      mojom::VRFieldOfView::New(kFov, kFov, kFov, kFov);
  display_info->right_eye->field_of_view =
      display_info->left_eye->field_of_view->Clone();

  constexpr uint32_t kDimension = 1024;
  display_info->left_eye->render_width = kDimension;
  display_info->left_eye->render_height = kDimension;
  display_info->right_eye->render_width = kDimension;
  display_info->right_eye->render_width = kDimension;

  constexpr float kInterpupillaryDistance = 0.1f;  // 10cm
  display_info->left_eye->offset = {-kInterpupillaryDistance * 0.5, 0, 0};
  display_info->right_eye->offset = {kInterpupillaryDistance * 0.5, 0, 0};

  return display_info;
}

}  // namespace

OpenXRDevice::OpenXRDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      exclusive_controller_binding_(this),
      gamepad_provider_factory_binding_(this),
      compositor_host_binding_(this) {
  SetVRDisplayInfo(CreateFakeVRDisplayInfo(GetId()));
}

OpenXRDevice::~OpenXRDevice() = default;

bool OpenXRDevice::IsHardwareAvailable() {
  // TODO(https://crbug.com/976436)
  return true;
}

bool OpenXRDevice::IsApiAvailable() {
  // TODO(https://crbug.com/976436)
  return true;
}

mojom::IsolatedXRGamepadProviderFactoryPtr OpenXRDevice::BindGamepadFactory() {
  mojom::IsolatedXRGamepadProviderFactoryPtr ret;
  gamepad_provider_factory_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

mojom::XRCompositorHostPtr OpenXRDevice::BindCompositorHost() {
  mojom::XRCompositorHostPtr ret;
  compositor_host_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

void OpenXRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  std::move(callback).Run(nullptr, nullptr);
}

void OpenXRDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  NOTREACHED();
}

void OpenXRDevice::GetIsolatedXRGamepadProvider(
    mojom::IsolatedXRGamepadProviderRequest provider_request) {}

void OpenXRDevice::CreateImmersiveOverlay(
    mojom::ImmersiveOverlayRequest overlay_request) {}

}  // namespace device
