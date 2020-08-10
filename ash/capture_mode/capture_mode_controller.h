// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/public/cpp/capture_mode_delegate.h"

namespace ash {

class CaptureModeSession;

// Controls starting and ending a Capture Mode session and its behavior.
class ASH_EXPORT CaptureModeController {
 public:
  explicit CaptureModeController(std::unique_ptr<CaptureModeDelegate> delegate);
  CaptureModeController(const CaptureModeController&) = delete;
  CaptureModeController& operator=(const CaptureModeController&) = delete;
  ~CaptureModeController();

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static CaptureModeController* Get();

  CaptureModeType type() const { return type_; }
  CaptureModeSource source() const { return source_; }
  CaptureModeSession* capture_mode_session() const {
    return capture_mode_session_.get();
  }

  // Returns true if a capture mode session is currently active.
  bool IsActive() const { return !!capture_mode_session_; }

  // Sets the capture source/type, which will be applied to an ongoing capture
  // session (if any), or to a future capture session when Start() is called.
  void SetSource(CaptureModeSource source);
  void SetType(CaptureModeType type);

  // Starts a new capture session with the most-recently used |type_| and
  // |source_|.
  void Start();

  // Stops an existing capture session.
  void Stop();

  void EndVideoRecording();

 private:
  std::unique_ptr<CaptureModeDelegate> delegate_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;

  std::unique_ptr<CaptureModeSession> capture_mode_session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
