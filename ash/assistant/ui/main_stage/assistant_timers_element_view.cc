// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_timers_element_view.h"

#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/assistant/model/ui/assistant_timers_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

// AssistantTimerView ----------------------------------------------------------

class AssistantTimerView : public views::View,
                           public AssistantAlarmTimerModelObserver {
 public:
  AssistantTimerView(AssistantViewDelegate* delegate,
                     const std::string& timer_id)
      : delegate_(delegate), timer_id_(timer_id) {
    InitLayout();
    UpdateLayout();

    delegate_->AddAlarmTimerModelObserver(this);
  }

  AssistantTimerView(const AssistantTimerView&) = delete;
  AssistantTimerView& operator=(const AssistantTimerView&) = delete;

  ~AssistantTimerView() override {
    delegate_->RemoveAlarmTimerModelObserver(this);
  }

  // views::View:
  const char* GetClassName() const override { return "AssistantTimerView"; }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  std::string ToStringForTesting() const {
    return base::UTF16ToUTF8(label_->GetText());
  }

 private:
  // AssistantAlarmTimerModelObserver:
  void OnTimerUpdated(const mojom::AssistantTimer& timer) override {
    if (timer.id == timer_id_)
      UpdateLayout();
  }

  // TODO(dmblack): Update w/ actual UI adhering to the spec.
  void InitLayout() {
    // Layout.
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // Label.
    label_ = AddChildView(std::make_unique<views::Label>());
  }

  // TODO(dmblack): Update w/ actual UI adhering to the spec.
  void UpdateLayout() {
    // NOTE: The timer for |timer_id| may no longer exist in the model if it has
    // been removed while Assistant UI is still showing. This will be better
    // handled in production once the UI spec has been implemented.
    const auto* timer =
        delegate_->GetAlarmTimerModel()->GetTimerById(timer_id_);
    const base::TimeDelta remaining_time =
        timer ? timer->remaining_time : base::TimeDelta();

    // Update |label_| to reflect remaining time.
    label_->SetText(
        base::UTF8ToUTF16(base::NumberToString(remaining_time.InSeconds())));
  }

  AssistantViewDelegate* const delegate_;  // Owned (indirectly) by Shell.
  views::Label* label_;                    // Owned by view hierarchy.

  const std::string timer_id_;
};

// AssistantTimersElementView --------------------------------------------------

AssistantTimersElementView::AssistantTimersElementView(
    AssistantViewDelegate* delegate,
    const AssistantTimersElement* timers_element) {
  InitLayout(delegate, timers_element);
}

AssistantTimersElementView::~AssistantTimersElementView() {
  scroll_view_->RemoveScrollViewObserver(this);
}

const char* AssistantTimersElementView::GetClassName() const {
  return "AssistantTimersElementView";
}

ui::Layer* AssistantTimersElementView::GetLayerForAnimating() {
  return layer();
}

std::string AssistantTimersElementView::ToStringForTesting() const {
  std::stringstream result;
  for (auto* child : scroll_view_->content_view()->children()) {
    AssistantTimerView* timer_view = static_cast<AssistantTimerView*>(child);
    result << timer_view->ToStringForTesting() << "\n";
  }
  return result.str();
}

gfx::Size AssistantTimersElementView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantTimersElementView::GetHeightForWidth(int width) const {
  return scroll_view_->content_view()->GetHeightForWidth(width);
}

void AssistantTimersElementView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantTimersElementView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  int width = std::max(content_view->GetPreferredSize().width(), this->width());
  int height = content_view->GetHeightForWidth(width);
  content_view->SetSize(gfx::Size(width, height));
}

void AssistantTimersElementView::InitLayout(
    AssistantViewDelegate* delegate,
    const AssistantTimersElement* timers_element) {
  // Layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Layout.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Scroll view.
  scroll_view_ = AddChildView(std::make_unique<AssistantScrollView>());
  scroll_view_->AddScrollViewObserver(this);

  auto* lm = scroll_view_->content_view()->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kSpacingDip));
  lm->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  lm->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

  // Timers.
  for (const auto& timer_id : timers_element->timer_ids()) {
    scroll_view_->content_view()->AddChildView(
        std::make_unique<AssistantTimerView>(delegate, timer_id));
  }
}

}  // namespace ash
