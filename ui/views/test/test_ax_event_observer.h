// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_AX_EVENT_OBSERVER_H_
#define UI_VIEWS_TEST_TEST_AX_EVENT_OBSERVER_H_

#include "ui/views/accessibility/ax_event_observer.h"

namespace views {
namespace test {

// Observes all Views accessibility events for tests.
class TestAXEventObserver : public AXEventObserver {
 public:
  TestAXEventObserver();
  TestAXEventObserver(const TestAXEventObserver&) = delete;
  TestAXEventObserver& operator=(const TestAXEventObserver&) = delete;
  ~TestAXEventObserver() override;

  // AXEventObserver:
  void OnViewEvent(View* view, ax::mojom::Event event_type) override;

  int text_changed_event_count() const { return text_changed_event_count_; }

 private:
  int text_changed_event_count_ = 0;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_AX_EVENT_OBSERVER_H_
