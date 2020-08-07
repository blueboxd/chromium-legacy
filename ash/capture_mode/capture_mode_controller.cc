// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <utility>

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
  source_ = source;
  // TODO(afakhry): Fill in here.
}

void CaptureModeController::SetType(CaptureModeType type) {
  type_ = type;
  // TODO(afakhry): Fill in here.
}

void CaptureModeController::Start() {
  StartWith(type_, source_);
}

void CaptureModeController::StartWith(CaptureModeType type,
                                      CaptureModeSource source) {
  type_ = type;
  source_ = source;
  // TODO(afakhry): Fill in here.
}

void CaptureModeController::Stop() {
  // TODO(afakhry): Fill in here.
}

}  // namespace ash
