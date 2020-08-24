// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/ax_event_counter.h"

namespace views {
namespace test {

AXEventCounter::AXEventCounter(views::AXEventManager* event_manager)
    : tree_observer_(this) {
  tree_observer_.Add(event_manager);
}

AXEventCounter::~AXEventCounter() = default;

void AXEventCounter::OnViewEvent(views::View*, ax::mojom::Event event_type) {
  ++event_counts_[event_type];
}

int AXEventCounter::GetCount(ax::mojom::Event event_type) {
  return event_counts_[event_type];
}

}  // namespace test
}  // namespace views
