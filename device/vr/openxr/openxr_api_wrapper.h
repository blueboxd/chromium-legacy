// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
#define DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_

#include <d3d11.h>
#include <stdint.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "third_party/openxr/include/openxr/openxr.h"
#include "third_party/openxr/include/openxr/openxr_platform.h"

namespace gfx {
class Quaternion;
class Point3F;
}  // namespace gfx

namespace device {

class OpenXrApiWrapper {
 public:
  OpenXrApiWrapper();
  ~OpenXrApiWrapper();

  static std::unique_ptr<OpenXrApiWrapper> Create();

  static bool IsHardwareAvailable();
  static bool IsApiAvailable();

  XrResult StartSession(const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device);

  XrResult BeginFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D>* texture);
  XrResult EndFrame();

  XrResult GetHeadPose(gfx::Quaternion* orientation,
                       gfx::Point3F* position) const;

  XrTime GetPredictedDisplayTime() const;

  void GetViewSize(uint32_t* width, uint32_t* height) const;
  XrResult GetLuid(LUID* luid) const;

 private:
  void Reset();
  bool Initialize();
  void Uninitialize();
  bool IsInitialized() const;

  XrResult InitializeSystem();
  XrResult PickEnvironmentBlendMode(XrSystemId system);

  XrResult CreateSession(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device);
  XrResult CreateSwapchain();
  XrResult CreateSpace(XrReferenceSpaceType type, XrSpace* space);
  XrResult CreateViewSpace();

  XrResult BeginSession();
  XrResult UpdateProjectionLayers();

  bool HasInstance() const;
  bool HasSystem() const;
  bool HasBlendMode() const;
  bool HasSession() const;
  bool HasColorSwapChain() const;
  bool HasSpace(XrReferenceSpaceType type) const;
  bool HasFrameState() const;

  uint32_t GetRecommendedSwapchainSampleCount() const;

  // OpenXR objects

  // These objects are valid on successful initialization.
  XrInstance instance_;
  XrSystemId system_;
  std::vector<XrViewConfigurationView> view_configs_;
  XrEnvironmentBlendMode blend_mode_;

  // These objects are valid only while a session is active,
  // and stay constant throughout the lifetime of a session.
  XrSession session_;
  XrSwapchain color_swapchain_;
  std::vector<XrSwapchainImageD3D11KHR> color_swapchain_images_;
  XrSpace local_space_;
  XrSpace view_space_;

  // These objects store information about the current frame. They're
  // valid only while a session is active, and they are updated each frame.
  XrFrameState frame_state_;
  std::vector<XrView> views_;
  std::vector<XrCompositionLayerProjectionView> layer_projection_views_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrApiWrapper);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
