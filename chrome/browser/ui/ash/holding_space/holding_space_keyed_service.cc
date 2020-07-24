// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/guid.h"

namespace ash {

HoldingSpaceKeyedService::HoldingSpaceKeyedService(
    content::BrowserContext* context) {}

HoldingSpaceKeyedService::~HoldingSpaceKeyedService() = default;

void HoldingSpaceKeyedService::AddTextItem(const base::string16& text) {
  auto item = std::make_unique<HoldingSpaceItem>(base::GenerateGUID());
  item->set_text(text);
  holding_space_model_.AddItem(std::move(item));
}

void HoldingSpaceKeyedService::ActivateModel() {
  HoldingSpaceController::Get()->SetModel(&holding_space_model_);
}

}  // namespace ash
