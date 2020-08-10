// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <memory>
#include <utility>

#include "ash/capture_mode/capture_mode_session.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

CaptureModeController* g_instance = nullptr;

}  // namespace

CaptureModeController::CaptureModeController(
    std::unique_ptr<CaptureModeDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

CaptureModeController::~CaptureModeController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CaptureModeController* CaptureModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void CaptureModeController::SetSource(CaptureModeSource source) {
  if (source == source_)
    return;

  source_ = source;
  if (capture_mode_session_)
    capture_mode_session_->OnCaptureSourceChanged(source_);
}

void CaptureModeController::SetType(CaptureModeType type) {
  if (type == type_)
    return;

  type_ = type;
  if (capture_mode_session_)
    capture_mode_session_->OnCaptureTypeChanged(type_);
}

void CaptureModeController::Start() {
  if (capture_mode_session_)
    return;

  // TODO(afakhry): Use root window of the mouse cursor or the one for new
  // windows.
  capture_mode_session_ =
      std::make_unique<CaptureModeSession>(Shell::GetPrimaryRootWindow());
}

void CaptureModeController::Stop() {
  capture_mode_session_.reset();
}

void CaptureModeController::EndVideoRecording() {
  // TODO(afakhry): Fill in here.
}

}  // namespace ash
