// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

namespace device {

FakeVRDevice::FakeVRDevice(mojom::XRDeviceId id)
    : VRDeviceBase(id), controller_binding_(this) {
  SetVRDisplayInfo(InitBasicDevice());
}

FakeVRDevice::~FakeVRDevice() {}

mojom::VRDisplayInfoPtr FakeVRDevice::InitBasicDevice() {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = GetId();
  display_info->displayName = "FakeVRDevice";

  display_info->capabilities = mojom::VRDisplayCapabilities::New();
  display_info->capabilities->hasPosition = false;
  display_info->capabilities->hasExternalDisplay = false;
  display_info->capabilities->canPresent = false;

  display_info->leftEye = InitEye(45, -0.03f, 1024);
  display_info->rightEye = InitEye(45, 0.03f, 1024);
  return display_info;
}

mojom::VREyeParametersPtr FakeVRDevice::InitEye(float fov,
                                                float offset,
                                                uint32_t size) {
  mojom::VREyeParametersPtr eye = mojom::VREyeParameters::New();

  eye->fieldOfView = mojom::VRFieldOfView::New();
  eye->fieldOfView->upDegrees = fov;
  eye->fieldOfView->downDegrees = fov;
  eye->fieldOfView->leftDegrees = fov;
  eye->fieldOfView->rightDegrees = fov;

  eye->offset = gfx::Vector3dF(offset, 0.0f, 0.0f);

  eye->renderWidth = size;
  eye->renderHeight = size;

  return eye;
}

void FakeVRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  OnStartPresenting();
  // The current tests never use the return values, so it's fine to return
  // invalid data here.
  std::move(callback).Run(nullptr, nullptr);
}

void FakeVRDevice::OnPresentingControllerMojoConnectionError() {
  OnExitPresent();
  controller_binding_.Close();
}

void FakeVRDevice::OnGetInlineFrameData(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->pose = pose_.Clone();
  std::move(callback).Run(std::move(frame_data));
}

}  // namespace device
