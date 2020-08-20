// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_ax_event_observer.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"

namespace views {
namespace test {

TestAXEventObserver::TestAXEventObserver() {
  AXEventManager::Get()->AddObserver(this);
}

TestAXEventObserver::~TestAXEventObserver() {
  AXEventManager::Get()->RemoveObserver(this);
}

void TestAXEventObserver::OnViewEvent(View* view, ax::mojom::Event event_type) {
  if (event_type == ax::mojom::Event::kTextChanged)
    ++text_changed_event_count_;
}

}  // namespace test
}  // namespace views
