// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_render_loop.h"

#include "device/vr/openxr/openxr_api_wrapper.h"

namespace device {

OpenXrRenderLoop::OpenXrRenderLoop() : XRCompositorCommon() {}

OpenXrRenderLoop::~OpenXrRenderLoop() {
  Stop();
}

void OpenXrRenderLoop::GetViewSize(uint32_t* width, uint32_t* height) const {
  openxr_->GetViewSize(width, height);
}

mojom::XRFrameDataPtr OpenXrRenderLoop::GetNextFrameData() {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

  if (XR_FAILED(openxr_->BeginFrame(&texture))) {
    return nullptr;
  }

  texture_helper_.SetBackbuffer(texture.Get());

  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->frame_id = next_frame_id_;

  frame_data->time_delta =
      base::TimeDelta::FromNanoseconds(openxr_->GetPredictedDisplayTime());

  gfx::Quaternion orientation;
  gfx::Point3F position;
  if (XR_SUCCEEDED(openxr_->GetHeadPose(&orientation, &position))) {
    frame_data->pose = mojom::VRPose::New();
    frame_data->pose->orientation = orientation;
    frame_data->pose->position = position;
  }

  return frame_data;
}

mojom::XRGamepadDataPtr OpenXrRenderLoop::GetNextGamepadData() {
  return nullptr;
}

bool OpenXrRenderLoop::StartRuntime() {
  DCHECK(!openxr_);

  // The new wrapper object is stored in a temporary variable instead of
  // openxr_ so that the local unique_ptr cleans up the object if starting
  // a session fails. openxr_ is set later in this method once we know
  // starting the session succeeds.
  std::unique_ptr<OpenXrApiWrapper> openxr = OpenXrApiWrapper::Create();
  if (!openxr)
    return false;

  texture_helper_.SetUseBGRA(true);
  LUID luid;
  if (XR_FAILED(openxr->GetLuid(&luid)) ||
      !texture_helper_.SetAdapterLUID(luid) ||
      !texture_helper_.EnsureInitialized() ||
      XR_FAILED(openxr->StartSession(texture_helper_.GetDevice()))) {
    texture_helper_.Reset();
    return false;
  }

  // Starting session succeeded so we can set the member variable.
  // Any additional code added below this should never fail.
  openxr_ = std::move(openxr);

  uint32_t width, height;
  GetViewSize(&width, &height);
  texture_helper_.SetDefaultSize(gfx::Size(width, height));

  DCHECK(openxr_);
  return true;
}

void OpenXrRenderLoop::StopRuntime() {
  openxr_ = nullptr;
  texture_helper_.Reset();
}

void OpenXrRenderLoop::OnSessionStart() {
  LogViewerType(VrViewerType::OPENXR_UNKNOWN);
}

bool OpenXrRenderLoop::PreComposite() {
  return true;
}

bool OpenXrRenderLoop::SubmitCompositedFrame() {
  return XR_SUCCEEDED(openxr_->EndFrame());
}

}  // namespace device
