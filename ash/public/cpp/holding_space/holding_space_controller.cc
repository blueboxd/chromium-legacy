// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_controller.h"

#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "base/check.h"

namespace ash {

namespace {

HoldingSpaceController* g_instance = nullptr;

}  // namespace

HoldingSpaceController::HoldingSpaceController() {
  CHECK(!g_instance);
  g_instance = this;
}

HoldingSpaceController::~HoldingSpaceController() {
  CHECK_EQ(g_instance, this);

  SetModel(nullptr);
  g_instance = nullptr;
}

// static
HoldingSpaceController* HoldingSpaceController::Get() {
  return g_instance;
}

void HoldingSpaceController::AddObserver(
    HoldingSpaceControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void HoldingSpaceController::RemoveObserver(
    HoldingSpaceControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HoldingSpaceController::SetModel(HoldingSpaceModel* model) {
  if (model_) {
    for (auto& observer : observers_)
      observer.OnHoldingSpaceModelDetached(model_);
  }

  model_ = model;

  if (model_) {
    for (auto& observer : observers_)
      observer.OnHoldingSpaceModelAttached(model_);
  }
}

}  // namespace ash
