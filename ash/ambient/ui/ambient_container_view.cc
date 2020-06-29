// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/ui/ambient_assistant_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/aura/window.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using chromeos::assistant::features::IsAmbientAssistantEnabled;

// Appearance.
constexpr int kHorizontalMarginDip = 16;
constexpr int kVerticalMarginDip = 64;
constexpr int kAssistantPreferredHeightDip = 128;

}  // namespace

// HostWidgetEventObserver----------------------------------

// A pre target event handler installed on the hosting widget of
// |AmbientContainerView| to capture key event regardless of whether
// |AmbientContainerView| has focus.
class AmbientContainerView::HostWidgetEventObserver : public ui::EventObserver {
 public:
  explicit HostWidgetEventObserver(AmbientContainerView* container)
      : container_(container) {
    DCHECK(container_);
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, container_->GetWidget()->GetNativeWindow(), {ui::ET_KEY_PRESSED});
  }

  ~HostWidgetEventObserver() override = default;

  HostWidgetEventObserver(const HostWidgetEventObserver&) = delete;
  HostWidgetEventObserver& operator=(const HostWidgetEventObserver&) = delete;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    DCHECK(event.type() == ui::ET_KEY_PRESSED);
    container_->HandleKeyEvent();
  }

 private:
  AmbientContainerView* const container_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

AmbientContainerView::AmbientContainerView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  Init();
}

AmbientContainerView::~AmbientContainerView() {
  event_observer_.reset();
}

const char* AmbientContainerView::GetClassName() const {
  return "AmbientContainerView";
}

gfx::Size AmbientContainerView::CalculatePreferredSize() const {
  // TODO(b/139953389): Handle multiple displays.
  return GetWidget()->GetNativeWindow()->GetRootWindow()->bounds().size();
}

void AmbientContainerView::Layout() {
  // Layout child views first to have proper bounds set for children.
  LayoutPhotoView();
  LayoutGlanceableInfoView();
  // The assistant view may not exist if |kAmbientAssistant| feature is
  // disabled.
  if (ambient_assistant_container_view_)
    LayoutAssistantView();

  View::Layout();
}

void AmbientContainerView::AddedToWidget() {
  event_observer_ = std::make_unique<HostWidgetEventObserver>(this);
}

void AmbientContainerView::Init() {
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  // Updates focus behavior to receive key press events.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  photo_view_ = AddChildView(std::make_unique<PhotoView>(delegate_));

  glanceable_info_view_ =
      AddChildView(std::make_unique<GlanceableInfoView>(delegate_));

  if (IsAmbientAssistantEnabled()) {
    ambient_assistant_container_view_ =
        AddChildView(std::make_unique<AmbientAssistantContainerView>());
    ambient_assistant_container_view_->SetVisible(false);
  }
}

void AmbientContainerView::LayoutPhotoView() {
  // |photo_view_| should have the same size as the widget.
  photo_view_->SetBoundsRect(GetLocalBounds());
}

void AmbientContainerView::LayoutGlanceableInfoView() {
  const gfx::Size container_size = GetLocalBounds().size();
  const gfx::Size preferred_size = glanceable_info_view_->GetPreferredSize();

  // The clock and weather view is positioned on the left-bottom corner of the
  // container.
  int x = kHorizontalMarginDip;
  int y =
      container_size.height() - kVerticalMarginDip - preferred_size.height();
  glanceable_info_view_->SetBoundsRect(
      gfx::Rect(x, y, preferred_size.width(), preferred_size.height()));
}

void AmbientContainerView::LayoutAssistantView() {
  int preferred_width = GetPreferredSize().width();
  int preferred_height = kAssistantPreferredHeightDip;
  ambient_assistant_container_view_->SetBoundsRect(
      gfx::Rect(0, 0, preferred_width, preferred_height));
}

void AmbientContainerView::HandleKeyEvent() {
  delegate_->OnBackgroundPhotoEvents();
}

}  // namespace ash
