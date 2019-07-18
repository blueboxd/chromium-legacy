// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
#define DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "device/vr/windows/compositor_base.h"

namespace device {

class OpenXrApiWrapper;

class OpenXrRenderLoop : public XRCompositorCommon {
 public:
  OpenXrRenderLoop();
  ~OpenXrRenderLoop() override;

  void GetViewSize(uint32_t* width, uint32_t* height) const;

 private:
  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  mojom::XRGamepadDataPtr GetNextGamepadData() override;
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool PreComposite() override;
  bool SubmitCompositedFrame() override;

  std::unique_ptr<OpenXrApiWrapper> openxr_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
