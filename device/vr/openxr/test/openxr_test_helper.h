// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_
#define DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_

#include <d3d11.h>
#include <unknwn.h>
#include <wrl.h>
#include <vector>

#include "base/synchronization/lock.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

class OpenXrTestHelper : public device::ServiceTestHook {
 public:
  OpenXrTestHelper();
  ~OpenXrTestHelper();

  void TestFailure();

  // TestHookRegistration
  void SetTestHook(device::VRTestHook* hook) final;

  // Helper methods called by the mock OpenXR runtime. These methods will
  // call back into the test hook, thus communicating with the test object
  // on the browser process side.
  void OnPresentedFrame();

  // Helper methods called by the mock OpenXR runtime to query or set the
  // state of the runtime.

  XrSystemId GetSystemId();
  XrSession GetSession();
  XrSwapchain GetSwapchain();
  XrSpace CreateLocalSpace();
  XrSpace CreateViewSpace();

  XrResult BeginSession();
  XrResult EndSession();

  void SetD3DDevice(ID3D11Device* d3d_device);
  const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>&
  GetSwapchainTextures() const;
  uint32_t NextSwapchainImageIndex();
  XrTime NextPredictedDisplayTime();

  void GetPose(XrPosef* pose);

  // Methods that validate the parameter with the current state of the runtime.
  XrResult ValidateInstance(XrInstance instance) const;
  XrResult ValidateSystemId(XrSystemId system_id) const;
  XrResult ValidateSession(XrSession session) const;
  XrResult ValidateSwapchain(XrSwapchain swapchain) const;
  XrResult ValidateSpace(XrSpace space) const;
  XrResult ValidatePredictedDisplayTime(XrTime time) const;
  XrResult ValidateXrPosefIsIdentity(const XrPosef& pose) const;

  // Properties of the mock OpenXR runtime that does not change are created
  // as static variables.
  static uint32_t NumExtensionsSupported();
  static uint32_t NumViews();
  static const char* kExtensions[];
  static const uint32_t kDimension;
  static const uint32_t kSwapCount;
  static const uint32_t kMinSwapchainBuffering;
  static const uint32_t kMaxViewCount;
  static const XrViewConfigurationView kViewConfigView;
  static XrViewConfigurationView kViewConfigurationViews[];
  static const XrViewConfigurationType kViewConfigurationType;
  static const XrEnvironmentBlendMode kEnvironmentBlendMode;

 private:
  // Properties of the mock OpenXR runtime that doesn't change throughout the
  // lifetime of the instance. However, these aren't static because they are
  // initialized to an invalid value and set to their actual value in their
  // respective Get*/Create* functions. This allows these variables to be used
  // to validate that they were queried before being used.
  XrSystemId system_id_;
  XrSession session_;
  XrSwapchain swapchain_;
  XrSpace local_space_;
  XrSpace view_space_;

  // Properties that changes depending on the state of the runtime.
  bool session_running_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> textures_arr_;
  uint32_t acquired_swapchain_texture_;
  uint32_t next_action_space_;
  XrTime next_predicted_display_time_;

  device::VRTestHook* test_hook_ GUARDED_BY(lock_) = nullptr;
  base::Lock lock_;
};

#endif  // DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_
