// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"

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
  gfx::Rect user_capture_region() const { return user_capture_region_; }
  void set_user_capture_region(const gfx::Rect& region) {
    user_capture_region_ = region;
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

  // Called only while a capture session is in progress to perform the actual
  // capture depending on the current selected |source_| and |type_|, and ends
  // the capture session.
  void PerformCapture();

  void EndVideoRecording();

 private:
  // Show notification for the newly taken screenshot or screen recording.
  void ShowNotification(const base::FilePath& screen_capture_path);
  void HandleNotificationClicked(const base::FilePath& screen_capture_path,
                                 base::Optional<int> button_index);

  std::unique_ptr<CaptureModeDelegate> delegate_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;

  // We remember the user selected capture region when the source is |kRegion|
  // between sessions. Initially, this value is empty at which point we display
  // a message to the user instructing them to start selecting a region.
  gfx::Rect user_capture_region_;

  std::unique_ptr<CaptureModeSession> capture_mode_session_;

  base::WeakPtrFactory<CaptureModeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
