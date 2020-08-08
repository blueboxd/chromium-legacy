// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Encapsulates an active capture mode session (i.e. an instance of this class
// lives as long as capture mode is active). It creates and owns the capture
// mode bar widget.
class ASH_EXPORT CaptureModeSession {
 public:
  // Creates the bar widget on the given |root| window.
  explicit CaptureModeSession(aura::Window* root);
  CaptureModeSession(const CaptureModeSession&) = delete;
  CaptureModeSession& operator=(const CaptureModeSession&) = delete;
  ~CaptureModeSession();

  CaptureModeBarView* capture_mode_bar_view() const {
    return capture_mode_bar_view_;
  }

  // Called when either the capture source or type changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);
  void OnCaptureTypeChanged(CaptureModeType new_type);

 private:
  views::Widget capture_mode_bar_widget_;

  // The content view of the above widget and owned by its views hierarchy.
  CaptureModeBarView* capture_mode_bar_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
