// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_controller.h"

#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/check.h"

namespace ash {

namespace {

HoldingSpaceController* g_instance = nullptr;

}  // namespace

HoldingSpaceController::HoldingSpaceController() {
  CHECK(!g_instance);
  g_instance = this;

  SessionController::Get()->AddObserver(this);
}

HoldingSpaceController::~HoldingSpaceController() {
  CHECK_EQ(g_instance, this);

  SetModel(nullptr);
  g_instance = nullptr;

  SessionController::Get()->RemoveObserver(this);
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

void HoldingSpaceController::RegisterModelForUser(const AccountId& account_id,
                                                  HoldingSpaceModel* model) {
  models_by_account_id_[account_id] = model;
  if (account_id == active_user_account_id_)
    SetModel(model);
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

void HoldingSpaceController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_user_account_id_ = account_id;

  auto model_it = models_by_account_id_.find(account_id);
  if (model_it == models_by_account_id_.end()) {
    SetModel(nullptr);
    return;
  }
  SetModel(model_it->second);
}

}  // namespace ash
