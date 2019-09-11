// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"

#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

bool IsScrollableShelfEnabled() {
  return chromeos::switches::ShouldShowScrollableShelf();
}

}  // namespace

class HotseatWidget::DelegateView : public views::WidgetDelegateView {
 public:
  explicit DelegateView() {}
  ~DelegateView() override {}

  // views::WidgetDelegateView:
  bool CanActivate() const override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

 private:
  FocusCycler* focus_cycler_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

bool HotseatWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
}

HotseatWidget::HotseatWidget() : delegate_view_(new DelegateView()) {}

void HotseatWidget::Initialize(aura::Window* container, Shelf* shelf) {
  DCHECK(container);
  DCHECK(shelf);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "HotseatWidget";
  params.delegate = delegate_view_;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);

  if (IsScrollableShelfEnabled()) {
    scrollable_shelf_view_ = GetContentsView()->AddChildView(
        std::make_unique<ScrollableShelfView>(ShelfModel::Get(), shelf));
    scrollable_shelf_view_->Init();
  } else {
    // The shelf view observes the shelf model and creates icons as items are
    // added to the model.
    shelf_view_ = GetContentsView()->AddChildView(
        std::make_unique<ShelfView>(ShelfModel::Get(), shelf));
    shelf_view_->Init();
  }

  delegate_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
}

void HotseatWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  views::Widget::OnMouseEvent(event);
}

void HotseatWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  GetShelfView()
      ->shelf_widget()
      ->shelf_layout_manager()
      ->ProcessGestureEventFromHotseatWidget(
          event, static_cast<aura::Window*>(event->target()));

  if (!event->handled())
    views::Widget::OnGestureEvent(event);
}

bool HotseatWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;

  if (IsScrollableShelfEnabled())
    scrollable_shelf_view_->OnFocusRingActivationChanged(active);
  else if (active)
    GetShelfView()->SetPaneFocusAndFocusDefault();

  return true;
}

ShelfView* HotseatWidget::GetShelfView() {
  if (IsScrollableShelfEnabled()) {
    DCHECK(scrollable_shelf_view_);
    return scrollable_shelf_view_->shelf_view();
  }

  DCHECK(shelf_view_);
  return shelf_view_;
}

const ShelfView* HotseatWidget::GetShelfView() const {
  return const_cast<const ShelfView*>(
      const_cast<HotseatWidget*>(this)->GetShelfView());
}

bool HotseatWidget::IsShowingOverflowBubble() const {
  return GetShelfView()->IsShowingOverflowBubble();
}

bool HotseatWidget::IsDraggedToExtended() const {
  DCHECK(GetShelfView()->shelf()->IsHorizontalAlignment());
  const int extended_y =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetShelfView()->GetWidget()->GetNativeView())
          .bounds()
          .bottom() -
      ShelfConfig::Get()->shelf_size() * 2;
  return GetWindowBoundsInScreen().y() == extended_y;
}

void HotseatWidget::FocusOverflowShelf(bool last_element) {
  if (!IsShowingOverflowBubble())
    return;
  Shell::Get()->focus_cycler()->FocusWidget(
      GetShelfView()->overflow_bubble()->bubble_view()->GetWidget());
  views::View* to_focus =
      GetShelfView()->overflow_shelf()->FindFirstOrLastFocusableChild(
          last_element);
  to_focus->RequestFocus();
}

void HotseatWidget::FocusFirstOrLastFocusableChild(bool last) {
  GetShelfView()->FindFirstOrLastFocusableChild(last)->RequestFocus();
}

void HotseatWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

}  // namespace ash
