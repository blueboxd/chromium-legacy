// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_PRE_TARGET_HANDLER_H_
#define ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_PRE_TARGET_HANDLER_H_

#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

class QuickAnswersView;

// This class handles mouse events, and update background color or
// dismiss quick answers view.
// TODO (siabhijeet): Migrate to using two-phased event dispatching.
class QuickAnswersPreTargetHandler : public ui::EventHandler {
 public:
  explicit QuickAnswersPreTargetHandler(QuickAnswersView* quick_answers_view);

  // Disallow copy and assign.
  QuickAnswersPreTargetHandler(const QuickAnswersPreTargetHandler&) = delete;
  QuickAnswersPreTargetHandler& operator=(const QuickAnswersPreTargetHandler&) =
      delete;

  ~QuickAnswersPreTargetHandler() override;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

 private:
  // Returns true if event was consumed by |view| or its children.
  bool DoDispatchEvent(views::View* view, ui::LocatedEvent* event);

  QuickAnswersView* quick_answers_view_;
};

}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_PRE_TARGET_HANDLER_H_
