// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_api_wrapper.h"

#include <directxmath.h>
#include <stdint.h>
#include <algorithm>
#include <array>

#include "base/logging.h"
#include "device/vr/openxr/openxr_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"

namespace device {

namespace {

constexpr XrSystemId kInvalidSystem = -1;
// Only supported view configuration:
constexpr XrViewConfigurationType kSupportedViewConfiguration =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
constexpr uint32_t kNumViews = 2;

XrResult CreateInstance(XrInstance* instance) {
  XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
  strcpy_s(instance_create_info.applicationInfo.applicationName, "Chromium");

  // xrCreateInstance validates the list of extensions and returns
  // XR_ERROR_EXTENSION_NOT_PRESENT if an extension is not supported,
  // so we don't need to call xrEnumerateInstanceExtensionProperties
  // to validate these extensions.
  const char* extensions[] = {
      XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
  };

  instance_create_info.enabledExtensionCount =
      sizeof(extensions) / sizeof(extensions[0]);
  instance_create_info.enabledExtensionNames = extensions;

  return xrCreateInstance(&instance_create_info, instance);
}

XrResult GetSystem(XrInstance instance, XrSystemId* system) {
  XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  return xrGetSystem(instance, &system_info, system);
}

}  // namespace

bool OpenXrApiWrapper::IsHardwareAvailable() {
  bool is_available = false;
  XrInstance instance = XR_NULL_HANDLE;
  XrSystemId system;

  if (XR_SUCCEEDED(CreateInstance(&instance)) &&
      XR_SUCCEEDED(GetSystem(instance, &system))) {
    is_available = true;
  }

  if (instance != XR_NULL_HANDLE)
    xrDestroyInstance(instance);

  // System is not allocated so does not need to be destroyed

  return is_available;
}

bool OpenXrApiWrapper::IsApiAvailable() {
  XrInstance instance;
  if (XR_SUCCEEDED(CreateInstance(&instance))) {
    xrDestroyInstance(instance);
    return true;
  }

  return false;
}

std::unique_ptr<OpenXrApiWrapper> OpenXrApiWrapper::Create() {
  std::unique_ptr<OpenXrApiWrapper> openxr =
      std::make_unique<OpenXrApiWrapper>();

  if (!openxr->Initialize()) {
    return nullptr;
  }

  return openxr;
}

OpenXrApiWrapper::OpenXrApiWrapper() = default;

OpenXrApiWrapper::~OpenXrApiWrapper() {
  Uninitialize();
}

void OpenXrApiWrapper::Reset() {
  local_space_ = XR_NULL_HANDLE;
  view_space_ = XR_NULL_HANDLE;
  color_swapchain_ = XR_NULL_HANDLE;
  session_ = XR_NULL_HANDLE;
  blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;
  system_ = kInvalidSystem;
  instance_ = XR_NULL_HANDLE;

  view_configs_.clear();
  color_swapchain_images_.clear();
  frame_state_ = {};
  views_.clear();
  layer_projection_views_.clear();
}

bool OpenXrApiWrapper::Initialize() {
  Reset();

  if (XR_FAILED(CreateInstance(&instance_))) {
    return false;
  }
  DCHECK(HasInstance());

  if (XR_FAILED(InitializeSystem())) {
    // When initialization fails, the caller should release this object, so we
    // don't need to destroy the instance created above as it is destroyed in
    // the destructor.
    DCHECK(!IsInitialized());
    return false;
  }

  DCHECK(IsInitialized());
  return true;
}

bool OpenXrApiWrapper::IsInitialized() const {
  return HasInstance() && HasSystem();
}

void OpenXrApiWrapper::Uninitialize() {
  // Destroying an instance in OpenXR also destroys all child objects of that
  // instance (including the session, swapchain, and spaces objects),
  // so they don't need to be manually destroyed.
  if (HasInstance()) {
    xrDestroyInstance(instance_);
  }

  Reset();
}

bool OpenXrApiWrapper::HasInstance() const {
  return instance_ != XR_NULL_HANDLE;
}

bool OpenXrApiWrapper::HasSystem() const {
  return system_ != kInvalidSystem && view_configs_.size() == kNumViews &&
         HasBlendMode();
}

bool OpenXrApiWrapper::HasBlendMode() const {
  return blend_mode_ != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;
}

bool OpenXrApiWrapper::HasSession() const {
  return session_ != XR_NULL_HANDLE;
}

bool OpenXrApiWrapper::HasColorSwapChain() const {
  return color_swapchain_ != XR_NULL_HANDLE &&
         color_swapchain_images_.size() > 0;
}

bool OpenXrApiWrapper::HasSpace(XrReferenceSpaceType type) const {
  switch (type) {
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      return local_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      return view_space_ != XR_NULL_HANDLE;
    default:
      NOTREACHED();
      return false;
  }
}

bool OpenXrApiWrapper::HasFrameState() const {
  return frame_state_.type == XR_TYPE_FRAME_STATE;
}

XrResult OpenXrApiWrapper::InitializeSystem() {
  DCHECK(HasInstance());
  DCHECK(!HasSystem());

  XrResult xr_result;

  XrSystemId system;
  RETURN_IF_XR_FAILED(GetSystem(instance_, &system));

  uint32_t view_count;
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurationViews(
      instance_, system, kSupportedViewConfiguration, 0, &view_count, nullptr));

  // It would be an error for an OpenXR runtime to return anything other than 2
  // views to an app that only requested
  // XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO.
  DCHECK(view_count == kNumViews);

  std::vector<XrViewConfigurationView> view_configs(view_count);
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurationViews(
      instance_, system, kSupportedViewConfiguration, view_count, &view_count,
      view_configs.data()));

  RETURN_IF_XR_FAILED(PickEnvironmentBlendMode(system));

  // Only assign the member variables on success. If any of the above XR calls
  // fail, the vector cleans up view_configs if necessary. system does not need
  // to be cleaned up because it is not allocated.
  system_ = system;
  view_configs_ = std::move(view_configs);

  return xr_result;
}

XrResult OpenXrApiWrapper::PickEnvironmentBlendMode(XrSystemId system) {
  const std::array<XrEnvironmentBlendMode, 2> kSupportedBlendMode = {
      XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
      XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
  };
  DCHECK(HasInstance());
  XrResult xr_result;

  uint32_t blend_mode_count;
  RETURN_IF_XR_FAILED(xrEnumerateEnvironmentBlendModes(
      instance_, system, 0, &blend_mode_count, nullptr));

  std::vector<XrEnvironmentBlendMode> blend_modes(blend_mode_count);
  RETURN_IF_XR_FAILED(
      xrEnumerateEnvironmentBlendModes(instance_, system, blend_mode_count,
                                       &blend_mode_count, blend_modes.data()));

  auto* blend_mode_it =
      std::find_first_of(kSupportedBlendMode.begin(), kSupportedBlendMode.end(),
                         blend_modes.begin(), blend_modes.end());
  if (blend_mode_it == kSupportedBlendMode.end()) {
    return XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED;
  }

  blend_mode_ = *blend_mode_it;
  return xr_result;
}

// Callers of this function must check the XrResult return value and destroy
// this OpenXrApiWrapper object on failure to clean up any intermediate
// objects that may have been created before the failure.
XrResult OpenXrApiWrapper::StartSession(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device) {
  DCHECK(d3d_device.Get());
  DCHECK(IsInitialized());

  XrResult xr_result;

  RETURN_IF_XR_FAILED(CreateSession(d3d_device));
  RETURN_IF_XR_FAILED(CreateSwapchain());
  RETURN_IF_XR_FAILED(
      CreateSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, &local_space_));
  RETURN_IF_XR_FAILED(CreateSpace(XR_REFERENCE_SPACE_TYPE_VIEW, &view_space_));
  RETURN_IF_XR_FAILED(BeginSession());

  // Since the objects in these arrays are used on every frame,
  // we don't want to create and destroy these objects every frame,
  // so create the number of objects we need and reuse them.
  views_.resize(view_configs_.size());
  layer_projection_views_.resize(view_configs_.size());

  // Make sure all of the objects we initialized are there.
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_VIEW));

  return xr_result;
}

XrResult OpenXrApiWrapper::CreateSession(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device) {
  DCHECK(d3d_device.Get());
  DCHECK(!HasSession());
  DCHECK(IsInitialized());

  XrGraphicsBindingD3D11KHR d3d11_binding = {
      XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
  d3d11_binding.device = d3d_device.Get();

  XrSessionCreateInfo session_create_info = {XR_TYPE_SESSION_CREATE_INFO};
  session_create_info.next = &d3d11_binding;
  session_create_info.systemId = system_;

  return xrCreateSession(instance_, &session_create_info, &session_);
}

XrResult OpenXrApiWrapper::CreateSwapchain() {
  DCHECK(IsInitialized());
  DCHECK(HasSession());
  DCHECK(!HasColorSwapChain());

  XrResult xr_result;

  uint32_t width, height;
  GetViewSize(&width, &height);

  XrSwapchainCreateInfo swapchain_create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swapchain_create_info.arraySize = 1;
  swapchain_create_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;

  // WebVR and WebXR textures are double wide, meaning the texture contains
  // both the left and the right eye, so the width of the swapchain texture
  // needs to be doubled.
  swapchain_create_info.width = width * 2;
  swapchain_create_info.height = height;
  swapchain_create_info.mipCount = 1;
  swapchain_create_info.faceCount = 1;
  swapchain_create_info.sampleCount = GetRecommendedSwapchainSampleCount();
  swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  XrSwapchain color_swapchain;
  RETURN_IF_XR_FAILED(
      xrCreateSwapchain(session_, &swapchain_create_info, &color_swapchain));

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));

  std::vector<XrSwapchainImageD3D11KHR> color_swapchain_images(chain_length);
  for (XrSwapchainImageD3D11KHR& image : color_swapchain_images) {
    image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
  }

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          color_swapchain_images.data())));

  color_swapchain_ = color_swapchain;
  color_swapchain_images_ = std::move(color_swapchain_images);
  return xr_result;
}

XrResult OpenXrApiWrapper::CreateSpace(XrReferenceSpaceType type,
                                       XrSpace* space) {
  DCHECK(HasSession());
  DCHECK(!HasSpace(type));

  XrReferenceSpaceCreateInfo space_create_info = {
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  space_create_info.referenceSpaceType = type;
  space_create_info.poseInReferenceSpace = PoseIdentity();

  return xrCreateReferenceSpace(session_, &space_create_info, space);
}

XrResult OpenXrApiWrapper::BeginSession() {
  DCHECK(HasSession());

  XrSessionBeginInfo session_begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
  session_begin_info.primaryViewConfigurationType = kSupportedViewConfiguration;

  return xrBeginSession(session_, &session_begin_info);
}

XrResult OpenXrApiWrapper::BeginFrame(
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* texture) {
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());

  XrResult xr_result;

  XrFrameWaitInfo wait_frame_info = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frame_state = {XR_TYPE_FRAME_STATE};
  RETURN_IF_XR_FAILED(xrWaitFrame(session_, &wait_frame_info, &frame_state));

  XrFrameBeginInfo begin_frame_info = {XR_TYPE_FRAME_BEGIN_INFO};
  RETURN_IF_XR_FAILED(xrBeginFrame(session_, &begin_frame_info));

  XrSwapchainImageAcquireInfo acquire_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  uint32_t color_swapchain_image_index;
  RETURN_IF_XR_FAILED(xrAcquireSwapchainImage(color_swapchain_, &acquire_info,
                                              &color_swapchain_image_index));

  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait_info.timeout = XR_INFINITE_DURATION;

  RETURN_IF_XR_FAILED(xrWaitSwapchainImage(color_swapchain_, &wait_info));
  RETURN_IF_XR_FAILED(UpdateProjectionLayers());

  *texture = color_swapchain_images_[color_swapchain_image_index].texture;
  frame_state_ = frame_state;

  return xr_result;
}

XrResult OpenXrApiWrapper::EndFrame() {
  DCHECK(HasBlendMode());
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasFrameState());

  XrResult xr_result;

  XrSwapchainImageReleaseInfo release_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  RETURN_IF_XR_FAILED(xrReleaseSwapchainImage(color_swapchain_, &release_info));

  XrCompositionLayerProjection multi_projection_layer = {
      XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  XrCompositionLayerProjection* multi_projection_layer_ptr =
      &multi_projection_layer;
  multi_projection_layer.space = local_space_;
  multi_projection_layer.viewCount = views_.size();
  multi_projection_layer.views = layer_projection_views_.data();

  XrFrameEndInfo end_frame_info = {XR_TYPE_FRAME_END_INFO};
  end_frame_info.environmentBlendMode = blend_mode_;
  end_frame_info.layerCount = 1;
  end_frame_info.layers =
      reinterpret_cast<const XrCompositionLayerBaseHeader* const*>(
          &multi_projection_layer_ptr);
  end_frame_info.displayTime = frame_state_.predictedDisplayTime;

  RETURN_IF_XR_FAILED(xrEndFrame(session_, &end_frame_info));

  return xr_result;
}

XrResult OpenXrApiWrapper::UpdateProjectionLayers() {
  XrResult xr_result;

  XrViewState view_state = {XR_TYPE_VIEW_STATE};

  XrViewLocateInfo view_locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
  view_locate_info.displayTime = frame_state_.predictedDisplayTime;
  view_locate_info.space = local_space_;

  uint32_t view_count = 0;
  RETURN_IF_XR_FAILED(xrLocateViews(session_, &view_locate_info, &view_state,
                                    views_.size(), &view_count, views_.data()));

  uint32_t width, height;
  GetViewSize(&width, &height);

  DCHECK(view_count <= views_.size());
  DCHECK(view_count <= layer_projection_views_.size());

  for (uint32_t view_index = 0; view_index < view_count; view_index++) {
    const XrView& view = views_[view_index];

    XrCompositionLayerProjectionView& layer_projection_view =
        layer_projection_views_[view_index];

    layer_projection_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    layer_projection_view.pose = view.pose;
    layer_projection_view.fov = view.fov;
    layer_projection_view.subImage.swapchain = color_swapchain_;
    // Since we're in double wide mode, the texture
    // array only has one texture and is always index 0.
    layer_projection_view.subImage.imageArrayIndex = 0;
    layer_projection_view.subImage.imageRect.extent.width = width;
    layer_projection_view.subImage.imageRect.extent.height = height;
    // x coordinates is 0 for first view, 0 + i*width for ith view.
    layer_projection_view.subImage.imageRect.offset.x = width * view_index;
    layer_projection_view.subImage.imageRect.offset.y = 0;
  }

  return xr_result;
}

// Returns the next predicted display time in nanoseconds.
XrTime OpenXrApiWrapper::GetPredictedDisplayTime() const {
  DCHECK(IsInitialized());
  DCHECK(HasFrameState());

  return frame_state_.predictedDisplayTime;
}

XrResult OpenXrApiWrapper::GetHeadPose(gfx::Quaternion* orientation,
                                       gfx::Point3F* position) const {
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_VIEW));

  XrResult xr_result;

  XrSpaceRelation relation = {XR_TYPE_SPACE_RELATION};
  RETURN_IF_XR_FAILED(xrLocateSpace(
      view_space_, local_space_, frame_state_.predictedDisplayTime, &relation));

  orientation->set_x(relation.pose.orientation.x);
  orientation->set_y(relation.pose.orientation.y);
  orientation->set_z(relation.pose.orientation.z);
  orientation->set_w(relation.pose.orientation.w);

  position->SetPoint(relation.pose.position.x, relation.pose.position.y,
                     relation.pose.position.z);

  return xr_result;
}

XrResult OpenXrApiWrapper::GetLuid(LUID* luid) const {
  DCHECK(IsInitialized());

  XrResult xr_result;

  XrGraphicsRequirementsD3D11KHR graphics_requirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
  RETURN_IF_XR_FAILED(xrGetD3D11GraphicsRequirementsKHR(
      instance_, system_, &graphics_requirements));

  luid->LowPart = graphics_requirements.adapterLuid.LowPart;
  luid->HighPart = graphics_requirements.adapterLuid.HighPart;

  return xr_result;
}

void OpenXrApiWrapper::GetViewSize(uint32_t* width, uint32_t* height) const {
  DCHECK(IsInitialized());
  CHECK(view_configs_.size() == kNumViews);

  *width = std::max(view_configs_[0].recommendedImageRectWidth,
                    view_configs_[1].recommendedImageRectWidth);
  *height = std::max(view_configs_[0].recommendedImageRectHeight,
                     view_configs_[1].recommendedImageRectHeight);
}

uint32_t OpenXrApiWrapper::GetRecommendedSwapchainSampleCount() const {
  DCHECK(IsInitialized());

  auto start = view_configs_.begin();
  auto end = view_configs_.end();

  auto compareSwapchainCounts = [](const XrViewConfigurationView& i,
                                   const XrViewConfigurationView& j) {
    return i.recommendedSwapchainSampleCount <
           j.recommendedSwapchainSampleCount;
  };

  return std::min_element(start, end, compareSwapchainCounts)
      ->recommendedSwapchainSampleCount;
}

}  // namespace device
