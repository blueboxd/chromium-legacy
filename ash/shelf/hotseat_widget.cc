// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"

#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

bool IsScrollableShelfEnabled() {
  return chromeos::switches::ShouldShowScrollableShelf();
}

bool ShouldShowHotseat() {
  return chromeos::switches::ShouldShowShelfHotseat() &&
         Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

class HotseatWidget::DelegateView : public views::WidgetDelegateView,
                                    public WallpaperControllerObserver {
 public:
  explicit DelegateView(WallpaperControllerImpl* wallpaper_controller)
      : opaque_background_(ui::LAYER_SOLID_COLOR),
        wallpaper_controller_(wallpaper_controller) {}
  ~DelegateView() override;

  // Initializes the view.
  void Init(ScrollableShelfView* scrollable_shelf_view,
            ui::Layer* parent_layer);
  // Updates the hotseat background.
  void UpdateOpaqueBackground();
  // Updates the hotseat background when tablet mode changes.
  void OnTabletModeChanged();

  // views::WidgetDelegateView:
  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }
 private:
  void SetParentLayer(ui::Layer* layer);
  FocusCycler* focus_cycler_ = nullptr;
  // A background layer that may be visible depending on HotseatState.
  ui::Layer opaque_background_;
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;  // unowned.
  // The WallpaperController, responsible for providing proper colors.
  WallpaperControllerImpl* wallpaper_controller_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

HotseatWidget::DelegateView::~DelegateView() {
  if (wallpaper_controller_)
    wallpaper_controller_->RemoveObserver(this);
}

void HotseatWidget::DelegateView::Init(
    ScrollableShelfView* scrollable_shelf_view,
    ui::Layer* parent_layer) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (!chromeos::switches::ShouldShowScrollableShelf())
    return;

  if (wallpaper_controller_)
    wallpaper_controller_->AddObserver(this);
  SetParentLayer(parent_layer);

  DCHECK(scrollable_shelf_view);
  scrollable_shelf_view_ = scrollable_shelf_view;
  UpdateOpaqueBackground();
}

void HotseatWidget::DelegateView::UpdateOpaqueBackground() {
  if (!ShouldShowHotseat()) {
    opaque_background_.SetVisible(false);
    return;
  }

  opaque_background_.SetVisible(true);
  opaque_background_.SetColor(ShelfConfig::Get()->GetDefaultShelfColor());

  const int radius = ShelfConfig::Get()->hotseat_size() / 2;
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (opaque_background_.rounded_corner_radii() != rounded_corners)
    opaque_background_.SetRoundedCornerRadius(rounded_corners);

  gfx::Rect background_bounds =
      scrollable_shelf_view_->GetHotseatBackgroundBounds();
  if (opaque_background_.bounds() != background_bounds)
    opaque_background_.SetBounds(background_bounds);

  opaque_background_.SetBackgroundBlur(ShelfConfig::Get()->shelf_blur_radius());
}

void HotseatWidget::DelegateView::OnTabletModeChanged() {
  UpdateOpaqueBackground();
}

bool HotseatWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
}

void HotseatWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  if (!chromeos::switches::ShouldShowScrollableShelf())
    return;

  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void HotseatWidget::DelegateView::OnWallpaperColorsChanged() {
  UpdateOpaqueBackground();
}

void HotseatWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

HotseatWidget::HotseatWidget()
    : delegate_view_(new DelegateView(Shell::Get()->wallpaper_controller())) {}

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
    shelf_view_ = GetContentsView()->AddChildView(std::make_unique<ShelfView>(
        ShelfModel::Get(), shelf, /*drag_and_drop_host=*/nullptr));
    shelf_view_->Init();
  }
  delegate_view_->Init(scrollable_shelf_view(), GetLayer());
}

void HotseatWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  views::Widget::OnMouseEvent(event);
}

void HotseatWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();

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
      (ShelfConfig::Get()->shelf_size() +
       ShelfConfig::Get()->hotseat_bottom_padding() +
       ShelfConfig::Get()->hotseat_size());
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

void HotseatWidget::OnTabletModeChanged() {
  GetShelfView()->OnTabletModeChanged();
  delegate_view_->OnTabletModeChanged();
}

void HotseatWidget::UpdateOpaqueBackground() {
  delegate_view_->UpdateOpaqueBackground();
}

void HotseatWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

}  // namespace ash
