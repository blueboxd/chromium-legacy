// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <directxmath.h>
#include <wrl.h>

#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/test/openxr_negotiate.h"
#include "device/vr/openxr/test/openxr_test_helper.h"

namespace {
// Global test helper that communicates with the test and contains the mock
// OpenXR runtime state/properties. A reference to this is returned as the
// instance handle through xrCreateInstance.
OpenXrTestHelper g_test_helper;
}  // namespace

// Mock implementations of openxr runtime.dll APIs.
// Please add new APIs in alphabetical order.

XrResult xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquire_info,
    uint32_t* index) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF_FALSE(acquire_info->type == XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrAcquireSwapchainImage type invalid");

  *index = g_test_helper.NextSwapchainImageIndex();

  return XR_SUCCESS;
}

XrResult xrBeginFrame(XrSession session,
                      const XrFrameBeginInfo* frame_begin_info) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(frame_begin_info->type == XR_TYPE_FRAME_BEGIN_INFO,
                  XR_ERROR_VALIDATION_FAILURE, "XrFrameBeginInfo type invalid");

  return XR_SUCCESS;
}

XrResult xrBeginSession(XrSession session,
                        const XrSessionBeginInfo* begin_info) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(begin_info->type == XR_TYPE_SESSION_BEGIN_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSessionBeginInfo type invalid");
  RETURN_IF_FALSE(begin_info->primaryViewConfigurationType ==
                      OpenXrTestHelper::kViewConfigurationType,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSessionBeginInfo primaryViewConfigurationType invalid");

  RETURN_IF_XR_FAILED(g_test_helper.BeginSession());

  return XR_SUCCESS;
}

XrResult xrCreateInstance(const XrInstanceCreateInfo* create_info,
                          XrInstance* instance) {
  DLOG(INFO) << __FUNCTION__;

  RETURN_IF_FALSE(create_info->type == XR_TYPE_INSTANCE_CREATE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrInstanceCreateInfo type invalid");

  RETURN_IF_FALSE(create_info->enabledExtensionCount ==
                      OpenXrTestHelper::NumExtensionsSupported(),
                  XR_ERROR_VALIDATION_FAILURE, "enabledExtensionCount invalid");

  for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
    bool valid_extension = false;
    for (size_t j = 0; j < OpenXrTestHelper::NumExtensionsSupported(); j++) {
      if (strcmp(create_info->enabledExtensionNames[i],
                 OpenXrTestHelper::kExtensions[j]) == 0) {
        valid_extension = true;
        break;
      }
    }

    RETURN_IF_FALSE(valid_extension, XR_ERROR_VALIDATION_FAILURE,
                    "enabledExtensionNames contains invalid extensions");
  }

  // Return the test helper object back to the OpenXrAPIWrapper so it can use
  // it as the TestHookRegistration.
  *instance = reinterpret_cast<XrInstance>(&g_test_helper);

  return XR_SUCCESS;
}

XrResult xrCreateReferenceSpace(XrSession session,
                                const XrReferenceSpaceCreateInfo* create_info,
                                XrSpace* space) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(create_info->type == XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrReferenceSpaceCreateInfo type invalid");
  RETURN_IF_FALSE(
      create_info->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL ||
          create_info->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW,
      XR_ERROR_VALIDATION_FAILURE,
      "XrReferenceSpaceCreateInfo referenceSpaceType invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateXrPosefIsIdentity(
      create_info->poseInReferenceSpace));

  switch (create_info->referenceSpaceType) {
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      *space = g_test_helper.CreateLocalSpace();
      break;
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      *space = g_test_helper.CreateViewSpace();
      break;
    default:
      RETURN_IF_FALSE(false, XR_ERROR_VALIDATION_FAILURE,
                      "XrReferenceSpaceCreateInfo referenceSpaceType invalid");
  }

  return XR_SUCCESS;
}

XrResult xrCreateSession(XrInstance instance,
                         const XrSessionCreateInfo* create_info,
                         XrSession* session) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_FALSE(create_info->type == XR_TYPE_SESSION_CREATE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSessionCreateInfo type invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(create_info->systemId));

  const XrGraphicsBindingD3D11KHR* binding =
      static_cast<const XrGraphicsBindingD3D11KHR*>(create_info->next);
  RETURN_IF_FALSE(binding->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrGraphicsBindingD3D11KHR type invalid");
  RETURN_IF_FALSE(binding->device != nullptr, XR_ERROR_VALIDATION_FAILURE,
                  "D3D11Device is null");

  g_test_helper.SetD3DDevice(binding->device);
  *session = g_test_helper.GetSession();

  return XR_SUCCESS;
}

XrResult xrCreateSwapchain(XrSession session,
                           const XrSwapchainCreateInfo* create_info,
                           XrSwapchain* swapchain) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(create_info->type == XR_TYPE_SWAPCHAIN_CREATE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo type invalid");
  RETURN_IF_FALSE(create_info->arraySize == 1, XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo arraySize invalid");
  RETURN_IF_FALSE(create_info->format == DXGI_FORMAT_R8G8B8A8_UNORM,
                  XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED,
                  "XrSwapchainCreateInfo format unsupported");
  RETURN_IF_FALSE(create_info->width == OpenXrTestHelper::kDimension * 2,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo width is not dimension * 2");
  RETURN_IF_FALSE(create_info->height == OpenXrTestHelper::kDimension,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo height is not dimension");
  RETURN_IF_FALSE(create_info->mipCount == 1, XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo mipCount is not 1");
  RETURN_IF_FALSE(create_info->faceCount == 1, XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo faceCount is not 1");
  RETURN_IF_FALSE(create_info->sampleCount == OpenXrTestHelper::kSwapCount,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSwapchainCreateInfo sampleCount invalid");
  RETURN_IF_FALSE(
      create_info->usageFlags == XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
      XR_ERROR_VALIDATION_FAILURE,
      "XrSwapchainCreateInfo usageFlags is not "
      "XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT");

  *swapchain = g_test_helper.GetSwapchain();

  return XR_SUCCESS;
}

XrResult xrDestroyInstance(XrInstance instance) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));

  return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frame_end_info) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(frame_end_info->type == XR_TYPE_FRAME_END_INFO,
                  XR_ERROR_VALIDATION_FAILURE, "frame_end_info type invalid");
  RETURN_IF_FALSE(frame_end_info->environmentBlendMode ==
                      OpenXrTestHelper::kEnvironmentBlendMode,
                  XR_ERROR_VALIDATION_FAILURE,
                  "frame_end_info environmentBlendMode invalid");
  RETURN_IF_FALSE(frame_end_info->layerCount == 1, XR_ERROR_VALIDATION_FAILURE,
                  "frame_end_info layerCount invalid");
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidatePredictedDisplayTime(frame_end_info->displayTime));

  g_test_helper.OnPresentedFrame();

  return XR_SUCCESS;
}

XrResult xrEndSession(XrSession session) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_XR_FAILED(g_test_helper.EndSession());

  return XR_SUCCESS;
}

XrResult xrEnumerateEnvironmentBlendModes(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    uint32_t environmentBlendModeCapacityInput,
    uint32_t* environmentBlendModeCountOutput,
    XrEnvironmentBlendMode* environmentBlendModes) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF_FALSE(
      view_configuration_type == OpenXrTestHelper::kViewConfigurationType,
      XR_ERROR_VALIDATION_FAILURE,
      "xrEnumerateEnvironmentBlendModes viewConfigurationType invalid");

  *environmentBlendModeCountOutput = 1;

  if (environmentBlendModeCapacityInput != 0) {
    *environmentBlendModes = OpenXrTestHelper::kEnvironmentBlendMode;
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateInstanceExtensionProperties(
    const char* layer_name,
    uint32_t property_capacity_input,
    uint32_t* property_count_output,
    XrExtensionProperties* properties) {
  DLOG(INFO) << __FUNCTION__;

  RETURN_IF_FALSE(
      property_capacity_input >= OpenXrTestHelper::NumExtensionsSupported() ||
          property_capacity_input == 0,
      XR_ERROR_SIZE_INSUFFICIENT, "XrExtensionProperties array is too small");

  *property_count_output = OpenXrTestHelper::NumExtensionsSupported();

  if (property_capacity_input != 0) {
    for (uint32_t i = 0; i < OpenXrTestHelper::NumExtensionsSupported(); i++) {
      errno_t error = strcpy_s(properties[i].extensionName,
                               OpenXrTestHelper::kExtensions[i]);
      DCHECK(error == 0);
      properties[i].extensionVersion = 1;
    }
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateViewConfigurationViews(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    uint32_t view_capacity_input,
    uint32_t* view_count_output,
    XrViewConfigurationView* views) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF_FALSE(
      view_configuration_type == OpenXrTestHelper::kViewConfigurationType,
      XR_ERROR_VALIDATION_FAILURE,
      "xrEnumerateViewConfigurationViews viewConfigurationType invalid");

  *view_count_output = OpenXrTestHelper::NumViews();

  if (view_capacity_input != 0) {
    views[0] = OpenXrTestHelper::kViewConfigurationViews[0];
    views[1] = OpenXrTestHelper::kViewConfigurationViews[1];
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain,
                                    uint32_t image_capacity_input,
                                    uint32_t* image_count_output,
                                    XrSwapchainImageBaseHeader* images) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF_FALSE(
      image_capacity_input == OpenXrTestHelper::kMinSwapchainBuffering ||
          image_capacity_input == 0,
      XR_ERROR_SIZE_INSUFFICIENT,
      "xrEnumerateSwapchainImages does not equal length returned by "
      "xrCreateSwapchain");

  *image_count_output = OpenXrTestHelper::kMinSwapchainBuffering;

  if (image_capacity_input != 0) {
    const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>& textures =
        g_test_helper.GetSwapchainTextures();
    DCHECK(textures.size() == image_capacity_input);

    for (uint32_t i = 0; i < image_capacity_input; i++) {
      XrSwapchainImageD3D11KHR& image =
          reinterpret_cast<XrSwapchainImageD3D11KHR*>(images)[i];

      RETURN_IF_FALSE(image.type == XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
                      XR_ERROR_VALIDATION_FAILURE,
                      "XrSwapchainImageD3D11KHR type invalid");

      image.texture = textures[i].Get();
    }
  }

  return XR_SUCCESS;
}

XrResult xrGetD3D11GraphicsRequirementsKHR(
    XrInstance instance,
    XrSystemId system_id,
    XrGraphicsRequirementsD3D11KHR* graphics_requirements) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF_FALSE(
      graphics_requirements->type == XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR,
      XR_ERROR_VALIDATION_FAILURE,
      "XrGraphicsRequirementsD3D11KHR type invalid");

  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  DCHECK(SUCCEEDED(hr));
  for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, &adapter)); i++) {
    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
    graphics_requirements->adapterLuid = desc.AdapterLuid;

    // Require D3D11.1 to support shared NT handles.
    graphics_requirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_1;

    return XR_SUCCESS;
  }

  RETURN_IF_FALSE(false, XR_ERROR_VALIDATION_FAILURE,
                  "Unable to create query DXGI Adapter");
}

XrResult xrGetSystem(XrInstance instance,
                     const XrSystemGetInfo* get_info,
                     XrSystemId* system_id) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_FALSE(get_info->type == XR_TYPE_SYSTEM_GET_INFO,
                  XR_ERROR_VALIDATION_FAILURE, "XrSystemGetInfo type invalid");
  RETURN_IF_FALSE(get_info->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XrSystemGetInfo formFactor invalid");

  *system_id = g_test_helper.GetSystemId();

  return XR_SUCCESS;
}

XrResult xrLocateSpace(XrSpace space,
                       XrSpace baseSpace,
                       XrTime time,
                       XrSpaceLocation* location) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(space));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(baseSpace));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePredictedDisplayTime(time));

  g_test_helper.GetPose(&(location->pose));

  location->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                            XR_SPACE_LOCATION_POSITION_VALID_BIT;

  return XR_SUCCESS;
}

XrResult xrLocateViews(XrSession session,
                       const XrViewLocateInfo* view_locate_info,
                       XrViewState* view_state,
                       uint32_t view_capacity_input,
                       uint32_t* view_count_output,
                       XrView* views) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePredictedDisplayTime(
      view_locate_info->displayTime));
  RETURN_IF_FALSE(view_locate_info->type == XR_TYPE_VIEW_LOCATE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrLocateViews view_locate_info type invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(view_locate_info->space));

  return XR_SUCCESS;
}

XrResult xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* release_info) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF_FALSE(release_info->type == XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrReleaseSwapchainImage type invalid");

  return XR_SUCCESS;
}

XrResult xrWaitFrame(XrSession session,
                     const XrFrameWaitInfo* frame_wait_info,
                     XrFrameState* frame_state) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(frame_wait_info->type == XR_TYPE_FRAME_WAIT_INFO,
                  XR_ERROR_VALIDATION_FAILURE, "frame_wait_info type invalid");
  RETURN_IF_FALSE(frame_state->type == XR_TYPE_FRAME_STATE,
                  XR_ERROR_VALIDATION_FAILURE,
                  "XR_TYPE_FRAME_STATE type invalid");

  frame_state->predictedDisplayTime = g_test_helper.NextPredictedDisplayTime();

  return XR_SUCCESS;
}

XrResult xrWaitSwapchainImage(XrSwapchain swapchain,
                              const XrSwapchainImageWaitInfo* wait_info) {
  DLOG(INFO) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF_FALSE(wait_info->type == XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrWaitSwapchainImage type invalid");
  RETURN_IF_FALSE(wait_info->timeout == XR_INFINITE_DURATION,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrWaitSwapchainImage timeout not XR_INFINITE_DURATION");

  return XR_SUCCESS;
}
