// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_
#define COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_

#include <vector>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "components/chromeos_camera/common/camera_app_helper.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromeos_camera {

class CameraAppHelperImpl : public ash::TabletModeObserver,
                            public mojom::CameraAppHelper {
 public:
  using CameraResultCallback =
      base::RepeatingCallback<void(uint32_t,
                                   arc::mojom::CameraIntentAction,
                                   const std::vector<uint8_t>&,
                                   HandleCameraResultCallback)>;
  using TabletModeMonitor = mojom::TabletModeMonitor;

  CameraAppHelperImpl(CameraResultCallback camera_result_callback,
                      aura::Window* window);
  ~CameraAppHelperImpl() override;

  // mojom::CameraAppHelper implementations.
  void HandleCameraResult(uint32_t intent_id,
                          arc::mojom::CameraIntentAction action,
                          const std::vector<uint8_t>& data,
                          HandleCameraResultCallback callback) override;
  void IsTabletMode(IsTabletModeCallback callback) override;
  void StartPerfEventTrace(const std::string& event) override;
  void StopPerfEventTrace(const std::string& event) override;
  void SetTabletMonitor(mojo::PendingRemote<TabletModeMonitor> monitor,
                        SetTabletMonitorCallback callback) override;

 private:
  // ash::TabletModeObserver overrides;
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  CameraResultCallback camera_result_callback_;

  mojo::Remote<TabletModeMonitor> monitor_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppHelperImpl);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_
