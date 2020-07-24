// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ash/holding_space/holding_space_presenter.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/stl_util.h"

namespace ash {

HoldingSpacePresenter::HoldingSpacePresenter() {
  HoldingSpaceController* const controller = HoldingSpaceController::Get();
  controller_observer_.Add(controller);
  if (controller->model())
    HandleNewModel(controller->model());
}

HoldingSpacePresenter::~HoldingSpacePresenter() = default;

void HoldingSpacePresenter::OnHoldingSpaceModelAttached(
    HoldingSpaceModel* model) {
  HandleNewModel(model);
}

void HoldingSpacePresenter::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Remove(model);
  item_ids_.clear();
}

void HoldingSpacePresenter::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  item_ids_.push_back(item->id());
}

void HoldingSpacePresenter::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  base::Erase(item_ids_, item->id());
}

void HoldingSpacePresenter::HandleNewModel(HoldingSpaceModel* model) {
  model_observer_.Add(model);

  for (auto& item : model->items())
    item_ids_.push_back(item->id());
}
}  // namespace ash
