// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_timers_element_view.h"

#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/ui/assistant_timers_element.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

AssistantTimersElementView::AssistantTimersElementView(
    AssistantViewDelegate* delegate,
    const AssistantTimersElement* timers_element)
    : delegate_(delegate), timers_element_(timers_element) {
  InitLayout();
  UpdateLayout();

  delegate_->AddAlarmTimerModelObserver(this);
}

AssistantTimersElementView::~AssistantTimersElementView() {
  delegate_->RemoveAlarmTimerModelObserver(this);
}

const char* AssistantTimersElementView::GetClassName() const {
  return "AssistantTimersElementView";
}

ui::Layer* AssistantTimersElementView::GetLayerForAnimating() {
  return layer();
}

std::string AssistantTimersElementView::ToStringForTesting() const {
  return base::UTF16ToUTF8(label_->GetText());
}

void AssistantTimersElementView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantTimersElementView::OnTimerUpdated(
    const mojom::AssistantTimer& timer) {
  UpdateLayout();
}

// TODO(dmblack): Update w/ actual UI adhering to the spec.
void AssistantTimersElementView::InitLayout() {
  // Layout.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Label.
  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetMultiLine(true);

  // Layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

// TODO(dmblack): Update w/ actual UI adhering to the spec.
void AssistantTimersElementView::UpdateLayout() {
  std::stringstream stream;

  for (const auto& timer_id : timers_element_->timer_ids()) {
    // NOTE: The timer for |timer_id| may no longer exist in the model if it
    // has been removed while Assistant UI is still showing. This will be better
    // handled in production once the UI spec has been implemented.
    const auto* timer = delegate_->GetAlarmTimerModel()->GetTimerById(timer_id);
    stream << (timer ? timer->remaining_time : base::TimeDelta()).InSeconds();
    stream << "\n";
  }

  label_->SetText(base::UTF8ToUTF16(stream.str()));
}

}  // namespace ash
