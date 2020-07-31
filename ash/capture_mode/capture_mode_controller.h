// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/capture_mode_delegate.h"

namespace ash {

// Defines the capture type Capture Mode is currently using.
enum class CaptureModeType {
  kImage,
  kVideo,
};

// Defines the source of the capture used by Capture Mode.
enum class CaptureModeSource {
  kFullscreen,
  kRegion,
  kWindow,
};

// Controls starting and ending a Capture Mode session and its behavior.
class CaptureModeController {
 public:
  explicit CaptureModeController(std::unique_ptr<CaptureModeDelegate> delegate);
  CaptureModeController(const CaptureModeController&) = delete;
  CaptureModeController& operator=(const CaptureModeController&) = delete;
  ~CaptureModeController();

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static CaptureModeController* Get();

  // Starts a new capture session with the most-recently used |type_| and
  // |source_|.
  void Start();

  // Starts a new capture session with the specific given |type| and |source|.
  void StartWith(CaptureModeType type, CaptureModeSource source);

  // Stops an existing capture session.
  void Stop();

 private:
  std::unique_ptr<CaptureModeDelegate> delegate_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
