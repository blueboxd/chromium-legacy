// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/test/openxr_test_helper.h"

#include <cmath>
#include <limits>

#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

// Initialize static variables in OpenXrTestHelper.
const char* OpenXrTestHelper::kExtensions[] = {
    XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
const uint32_t OpenXrTestHelper::kDimension = 128;
const uint32_t OpenXrTestHelper::kSwapCount = 1;
const uint32_t OpenXrTestHelper::kMinSwapchainBuffering = 3;
const uint32_t OpenXrTestHelper::kMaxViewCount = 2;
const XrViewConfigurationView OpenXrTestHelper::kViewConfigView = {
    XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr,
    OpenXrTestHelper::kDimension,    OpenXrTestHelper::kDimension,
    OpenXrTestHelper::kDimension,    OpenXrTestHelper::kDimension,
    OpenXrTestHelper::kSwapCount,    OpenXrTestHelper::kSwapCount};
XrViewConfigurationView OpenXrTestHelper::kViewConfigurationViews[] = {
    OpenXrTestHelper::kViewConfigView, OpenXrTestHelper::kViewConfigView};
const XrViewConfigurationType OpenXrTestHelper::kViewConfigurationType =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
const XrEnvironmentBlendMode OpenXrTestHelper::kEnvironmentBlendMode =
    XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

uint32_t OpenXrTestHelper::NumExtensionsSupported() {
  return sizeof(kExtensions) / sizeof(kExtensions[0]);
}

uint32_t OpenXrTestHelper::NumViews() {
  return sizeof(kViewConfigurationViews) / sizeof(kViewConfigurationViews[0]);
}

OpenXrTestHelper::OpenXrTestHelper()
    : system_id_(0),
      session_(XR_NULL_HANDLE),
      swapchain_(XR_NULL_HANDLE),
      local_space_(XR_NULL_HANDLE),
      view_space_(XR_NULL_HANDLE),
      session_running_(false),
      acquired_swapchain_texture_(0),
      next_action_space_(0),
      next_predicted_display_time_(0) {}

OpenXrTestHelper::~OpenXrTestHelper() = default;

void OpenXrTestHelper::TestFailure() {
  NOTREACHED();
}

void OpenXrTestHelper::SetTestHook(device::VRTestHook* hook) {
  base::AutoLock auto_lock(lock_);
  test_hook_ = hook;
}

void OpenXrTestHelper::OnPresentedFrame() {
  static uint32_t frame_id = 1;

  base::AutoLock auto_lock(lock_);
  if (!test_hook_)
    return;

  // TODO(https://crbug.com/986621): The frame color is currently hard-coded to
  // what the pixel tests expects. We should instead store the actual WebGL
  // texture and read from it, which will also verify the correct swapchain
  // texture was used.

  device::DeviceConfig device_config = test_hook_->WaitGetDeviceConfig();
  device::SubmittedFrameData frame_data = {};

  if (std::abs(device_config.interpupillary_distance - 0.2f) <
      std::numeric_limits<float>::epsilon()) {
    // TestPresentationPoses sets the ipd to 0.2f, whereas tests by default have
    // an ipd of 0.1f. This test has specific formulas to determine the colors,
    // specified in test_webxr_poses.html.
    frame_data.color = {
        frame_id % 256, ((frame_id - frame_id % 256) / 256) % 256,
        ((frame_id - frame_id % (256 * 256)) / (256 * 256)) % 256, 255};
  } else {
    // The WebXR tests by default clears to blue. TestPresentationPixels
    // verifies this color.
    frame_data.color = {0, 0, 255, 255};
  }

  frame_data.left_eye = true;
  test_hook_->OnFrameSubmitted(frame_data);

  frame_data.left_eye = false;
  test_hook_->OnFrameSubmitted(frame_data);

  frame_id++;
}

XrSystemId OpenXrTestHelper::GetSystemId() {
  system_id_ = 1;
  return system_id_;
}

XrSession OpenXrTestHelper::GetSession() {
  // reinterpret_cast needed because XrSession is a pointer type.
  session_ = reinterpret_cast<XrSession>(2);
  return session_;
}

XrSwapchain OpenXrTestHelper::GetSwapchain() {
  // reinterpret_cast needed because XrSwapchain is a pointer type.
  swapchain_ = reinterpret_cast<XrSwapchain>(3);
  return swapchain_;
}

XrSpace OpenXrTestHelper::CreateLocalSpace() {
  // reinterpret_cast needed because XrSpace is a pointer type.
  local_space_ = reinterpret_cast<XrSpace>(++next_action_space_);
  return local_space_;
}

XrSpace OpenXrTestHelper::CreateViewSpace() {
  // reinterpret_cast needed because XrSpace is a pointer type.
  view_space_ = reinterpret_cast<XrSpace>(++next_action_space_);
  return view_space_;
}

XrResult OpenXrTestHelper::BeginSession() {
  RETURN_IF_FALSE(!session_running_, XR_ERROR_SESSION_RUNNING,
                  "Session is already running");

  session_running_ = true;
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::EndSession() {
  RETURN_IF_FALSE(session_running_, XR_ERROR_SESSION_NOT_RUNNING,
                  "Session is not currently running");

  session_running_ = false;
  return XR_SUCCESS;
}

void OpenXrTestHelper::SetD3DDevice(ID3D11Device* d3d_device) {
  DCHECK(d3d_device_ == nullptr);
  DCHECK(d3d_device != nullptr);
  d3d_device_ = d3d_device;

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = kDimension * 2;  // Using a double wide texture
  desc.Height = kDimension;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;

  for (uint32_t i = 0; i < kMinSwapchainBuffering; i++) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3d_device_->CreateTexture2D(&desc, nullptr, &texture);
    DCHECK(hr == S_OK);

    textures_arr_.push_back(texture);
  }
}

const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>&
OpenXrTestHelper::GetSwapchainTextures() const {
  return textures_arr_;
}

uint32_t OpenXrTestHelper::NextSwapchainImageIndex() {
  acquired_swapchain_texture_ =
      (acquired_swapchain_texture_ + 1) % textures_arr_.size();
  return acquired_swapchain_texture_;
}

XrTime OpenXrTestHelper::NextPredictedDisplayTime() {
  return ++next_predicted_display_time_;
}

void OpenXrTestHelper::GetPose(XrPosef* pose) {
  *pose = device::PoseIdentity();

  base::AutoLock lock(lock_);
  if (test_hook_) {
    device::PoseFrameData pose_data = test_hook_->WaitGetPresentingPose();
    if (pose_data.is_valid) {
      gfx::Transform transform = PoseFrameDataToTransform(pose_data);

      gfx::DecomposedTransform decomposed_transform;
      bool decomposable =
          gfx::DecomposeTransform(&decomposed_transform, transform);
      DCHECK(decomposable);

      pose->orientation.x = decomposed_transform.quaternion.x();
      pose->orientation.y = decomposed_transform.quaternion.y();
      pose->orientation.z = decomposed_transform.quaternion.z();
      pose->orientation.w = decomposed_transform.quaternion.w();

      pose->position.x = decomposed_transform.translate[0];
      pose->position.y = decomposed_transform.translate[1];
      pose->position.z = decomposed_transform.translate[2];
    }
  }
}

XrResult OpenXrTestHelper::ValidateInstance(XrInstance instance) const {
  // The Fake OpenXR Runtime returns this global OpenXrTestHelper object as the
  // instance value on xrCreateInstance.
  RETURN_IF_FALSE(reinterpret_cast<OpenXrTestHelper*>(instance) == this,
                  XR_ERROR_VALIDATION_FAILURE, "XrInstance invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSystemId(XrSystemId system_id) const {
  RETURN_IF_FALSE(system_id_ != 0, XR_ERROR_SYSTEM_INVALID,
                  "XrSystemId has not been queried");
  RETURN_IF_FALSE(system_id == system_id_, XR_ERROR_SYSTEM_INVALID,
                  "XrSystemId invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSession(XrSession session) const {
  RETURN_IF_FALSE(session_ != XR_NULL_HANDLE, XR_ERROR_VALIDATION_FAILURE,
                  "XrSession has not been queried");
  RETURN_IF_FALSE(session == session_, XR_ERROR_VALIDATION_FAILURE,
                  "XrSession invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSwapchain(XrSwapchain swapchain) const {
  RETURN_IF_FALSE(swapchain_ != XR_NULL_HANDLE, XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchain has not been queried");
  RETURN_IF_FALSE(swapchain == swapchain_, XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchain invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSpace(XrSpace space) const {
  RETURN_IF_FALSE(space != XR_NULL_HANDLE, XR_ERROR_HANDLE_INVALID,
                  "XrSpace has not been queried");
  RETURN_IF_FALSE(reinterpret_cast<uint32_t>(space) <= next_action_space_,
                  XR_ERROR_HANDLE_INVALID, "XrSpace invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidatePredictedDisplayTime(XrTime time) const {
  RETURN_IF_FALSE(time != 0, XR_ERROR_VALIDATION_FAILURE,
                  "XrTime has not been queried");
  RETURN_IF_FALSE(time <= next_predicted_display_time_,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrTime predicted display time invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateXrPosefIsIdentity(
    const XrPosef& pose) const {
  XrPosef identity = device::PoseIdentity();
  bool is_identity = true;
  is_identity &= pose.orientation.x == identity.orientation.x;
  is_identity &= pose.orientation.y == identity.orientation.y;
  is_identity &= pose.orientation.z == identity.orientation.z;
  is_identity &= pose.orientation.w == identity.orientation.w;
  is_identity &= pose.position.x == identity.position.x;
  is_identity &= pose.position.y == identity.position.y;
  is_identity &= pose.position.z == identity.position.z;
  RETURN_IF_FALSE(is_identity, XR_ERROR_VALIDATION_FAILURE,
                  "XrPosef is not an identity");

  return XR_SUCCESS;
}
