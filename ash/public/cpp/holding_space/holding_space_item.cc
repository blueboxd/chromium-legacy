// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

namespace ash {

HoldingSpaceItem::HoldingSpaceItem(const std::string& id) : id_(id) {}

HoldingSpaceItem::~HoldingSpaceItem() = default;

}  // namespace ash
