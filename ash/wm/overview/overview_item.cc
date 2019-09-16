// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_label_widget.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

constexpr int kShadowElevation = 16;

// The amount of translation an item animates by when it is closed by using
// swipe to close.
constexpr int kSwipeToCloseCloseTranslationDp = 96;

// When an item is being dragged, the bounds are outset horizontally by this
// fraction of the width, and vertically by this fraction of the height. The
// outset in each dimension is on both sides, for a total of twice this much
// change in the size of the item along that dimension.
constexpr float kDragWindowScale = 0.05f;

constexpr int kCloseButtonInkDropInsetDp = 2;

constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

// The colors of the close button ripple.
constexpr SkColor kCloseButtonInkDropRippleColor =
    SkColorSetA(kCloseButtonColor, 0x0F);
constexpr SkColor kCloseButtonInkDropRippleHighlightColor =
    SkColorSetA(kCloseButtonColor, 0x14);

OverviewAnimationType GetExitOverviewAnimationTypeForMinimizedWindow(
    OverviewSession::EnterExitOverviewType type,
    bool should_animate_when_exiting) {
  // We should never get here when overview mode should exit immediately. The
  // minimized window's |item_widget_| should be closed and destroyed
  // immediately.
  DCHECK_NE(type, OverviewSession::EnterExitOverviewType::kImmediateExit);

  // EnterExitOverviewType can only be set to kWindowMinimized in talbet mode.
  // Fade out the minimized window without animation if switch from tablet mode
  // to clamshell mode.
  if (type == OverviewSession::EnterExitOverviewType::kSlideOutExit) {
    return Shell::Get()->tablet_mode_controller()->InTabletMode()
               ? OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER
               : OVERVIEW_ANIMATION_NONE;
  }
  return should_animate_when_exiting
             ? OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT
             : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

// Applies |new_bounds_in_screen| to |widget|, animating and observing the
// transform if necessary.
void SetWidgetBoundsAndMaybeAnimateTransform(
    views::Widget* widget,
    const gfx::Rect& new_bounds_in_screen,
    OverviewAnimationType animation_type,
    ui::ImplicitAnimationObserver* observer) {
  aura::Window* window = widget->GetNativeWindow();
  gfx::RectF previous_bounds = gfx::RectF(window->GetBoundsInScreen());
  window->SetBoundsInScreen(
      new_bounds_in_screen,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  if (animation_type == OVERVIEW_ANIMATION_NONE)
    return;

  // For animations, compute the transform needed to place the widget at its
  // new bounds back to the old bounds, and then apply the idenity
  // transform. This so the bounds visually line up the concurrent transform
  // animations. Also transform animations may be more performant.
  const gfx::RectF current_bounds = gfx::RectF(window->GetBoundsInScreen());
  window->SetTransform(
      gfx::TransformBetweenRects(current_bounds, previous_bounds));
  ScopedOverviewAnimationSettings settings(animation_type, window);
  if (observer)
    settings.AddObserver(observer);
  window->SetTransform(gfx::Transform());
}

}  // namespace

// The close button for the overview item. It has a custom ink drop.
class OverviewItem::OverviewCloseButton : public views::ImageButton {
 public:
  explicit OverviewCloseButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
    SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kOverviewWindowCloseIcon, kCloseButtonColor));
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetMinimumImageSize(gfx::Size(kHeaderHeightDp, kHeaderHeightDp));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  }

  ~OverviewCloseButton() override = default;

  // Resets the listener so that the listener can go out of scope.
  void ResetListener() { listener_ = nullptr; }

 protected:
  // views::Button:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    ink_drop->SetShowHighlightOnHover(true);
    return ink_drop;
  }
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
        kCloseButtonInkDropRippleColor, /*visible_opacity=*/1.f);
  }
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(
            kCloseButtonInkDropRippleHighlightColor, GetInkDropRadius()));
  }
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
  }

 private:
  int GetInkDropRadius() const {
    return std::min(size().width(), size().height()) / 2 -
           kCloseButtonInkDropInsetDp;
  }

  DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
};

OverviewItem::OverviewItem(aura::Window* window,
                           OverviewSession* overview_session,
                           OverviewGrid* overview_grid)
    : root_window_(window->GetRootWindow()),
      transform_window_(this, window),
      overview_session_(overview_session),
      overview_grid_(overview_grid) {
  CreateWindowLabel();
  for (auto* window_iter : WindowTransientDescendantIteratorRange(
           WindowTransientDescendantIterator(GetWindow()))) {
    window_iter->AddObserver(this);
  }
}

OverviewItem::~OverviewItem() {
  for (auto* window_iter : WindowTransientDescendantIteratorRange(
           WindowTransientDescendantIterator(GetWindow()))) {
    window_iter->RemoveObserver(this);
  }
}

aura::Window* OverviewItem::GetWindow() {
  return transform_window_.window();
}

bool OverviewItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void OverviewItem::RestoreWindow(bool reset_transform) {
  // TODO(oshima): SplitViewController has its own logic to adjust the
  // target state in |SplitViewController::OnOverviewModeEnding|.
  // Unify the mechanism to control it and remove ifs.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !Shell::Get()->split_view_controller()->InSplitViewMode() &&
      reset_transform) {
    MaximizeIfSnapped(GetWindow());
  }

  caption_container_view_->ResetEventDelegate();
  close_button_->ResetListener();
  transform_window_.RestoreWindow(reset_transform);

  if (transform_window_.IsMinimized()) {
    const auto enter_exit_type = overview_session_->enter_exit_overview_type();

    if (enter_exit_type ==
        OverviewSession::EnterExitOverviewType::kImmediateExit) {
      ImmediatelyCloseWidgetOnExit(std::move(item_widget_));
      return;
    }

    FadeOutWidgetAndMaybeSlideOnExit(
        std::move(item_widget_),
        GetExitOverviewAnimationTypeForMinimizedWindow(
            enter_exit_type, should_animate_when_exiting_));
  }
}

void OverviewItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

void OverviewItem::Shutdown() {
  item_widget_.reset();
  caption_container_view_ = nullptr;
}

void OverviewItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window, GetWindow());
}

void OverviewItem::SlideWindowIn() {
  // This only gets called if we see the home launcher on enter (all windows are
  // minimized).
  DCHECK(transform_window_.IsMinimized());

  // The mask and shadow will be shown when animation ends. Update the mask
  // after starting the animation since starting the animation lets the
  // controller know we are in starting animation.
  FadeInWidgetAndMaybeSlideOnEnter(item_widget_.get(),
                                   OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
                                   /*slide=*/true, /*observe=*/true);
  UpdateRoundedCornersAndShadow();
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
OverviewItem::UpdateYPositionAndOpacity(
    int new_grid_y,
    float opacity,
    OverviewSession::UpdateAnimationSettingsCallback callback) {
  aura::Window::Windows windows = GetWindowsForHomeGesture();
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings_to_observe;
  for (auto* window : windows) {
    ui::Layer* layer = window->layer();
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (!callback.is_null()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer->GetAnimator());
      callback.Run(settings.get());
    }
    layer->SetOpacity(opacity);

    int initial_y = 0;
    if (translation_y_map_.contains(window))
      initial_y = translation_y_map_[window];

    // Alter the y-translation. Offset by the window location relative to the
    // grid.
    gfx::Transform transform = layer->transform();
    transform.matrix().setFloat(1, 3,
                                static_cast<float>(initial_y - new_grid_y));
    layer->SetTransform(transform);

    // Return the first layer for the caller to observe.
    if (!settings_to_observe)
      settings_to_observe = std::move(settings);
  }

  return settings_to_observe;
}

void OverviewItem::UpdateItemContentViewForMinimizedWindow() {
  caption_container_view_->UpdatePreviewView();
}

float OverviewItem::GetItemScale(const gfx::Size& size) {
  gfx::SizeF inset_size(size.width(), size.height() - 2 * kWindowMargin);
  return ScopedOverviewTransformWindow::GetItemScale(
      GetTargetBoundsInScreen().size(), inset_size,
      transform_window_.GetTopInset(), kHeaderHeightDp);
}

gfx::RectF OverviewItem::GetTargetBoundsInScreen() const {
  return ::ash::GetTargetBoundsInScreen(transform_window_.window());
}

gfx::RectF OverviewItem::GetTransformedBounds() const {
  return transform_window_.GetTransformedBounds();
}

void OverviewItem::SetBounds(const gfx::RectF& target_bounds,
                             OverviewAnimationType animation_type) {
  if (in_bounds_update_ ||
      !Shell::Get()->overview_controller()->InOverviewSession())
    return;

  // Do not animate if the resulting bounds does not change. The original
  // window may change bounds so we still need to call SetItemBounds to update
  // the window transform.
  OverviewAnimationType new_animation_type = animation_type;
  if (target_bounds == target_bounds_ &&
      !GetWindow()->layer()->GetAnimator()->is_animating()) {
    new_animation_type = OVERVIEW_ANIMATION_NONE;
  }

  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  // If |target_bounds_| is empty, this is the first update. Let
  // UpdateHeaderLayout know, as we do not want |item_widget_| to be animated
  // with the window.
  const bool is_first_update = target_bounds_.IsEmpty();
  target_bounds_ = target_bounds;

  gfx::RectF inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);

  // If the window is minimized we can avoid applying transforms on the original
  // window.
  if (transform_window_.IsMinimized()) {
    item_widget_->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();

    gfx::Rect minimized_bounds = gfx::ToEnclosedRect(target_bounds);
    minimized_bounds.Inset(-kWindowMargin, -kWindowMargin);
    OverviewAnimationType minimized_animation_type =
        is_first_update ? OVERVIEW_ANIMATION_NONE : new_animation_type;
    SetWidgetBoundsAndMaybeAnimateTransform(
        item_widget_.get(), minimized_bounds, minimized_animation_type,
        minimized_animation_type ==
                OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW
            ? &transform_window_
            : nullptr);

    // On the first update show |item_widget_|. It's created on creation of
    // |this|, and needs to be shown as soon as its bounds have been determined
    // as it contains a mirror view of the window in its contents. The header
    // will be faded in later to match non minimized windows.
    if (is_first_update) {
      if (should_animate_when_entering_) {
        FadeInWidgetAndMaybeSlideOnEnter(
            item_widget_.get(), OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
            /*slide=*/false, /*observe=*/true);
      } else {
        item_widget_->GetNativeWindow()->layer()->SetOpacity(1.f);
      }
    }
  } else {
    // SetItemBounds is called before UpdateHeaderLayout so the header can
    // properly use the updated windows bounds.
    SetItemBounds(inset_bounds, new_animation_type);
    UpdateHeaderLayout(is_first_update ? OVERVIEW_ANIMATION_NONE
                                       : new_animation_type);
  }

  // Shadow is normally set after an animation is finished. In the case of no
  // animations, manually set the shadow. Shadow relies on both the window
  // transform and |item_widget_|'s new bounds so set it after SetItemBounds
  // and UpdateHeaderLayout. Do not apply the shadow for drop target.
  if (new_animation_type == OVERVIEW_ANIMATION_NONE)
    UpdateRoundedCornersAndShadow();

  if (cannot_snap_widget_) {
    inset_bounds.Inset(
        gfx::Insets(static_cast<float>(kHeaderHeightDp), 0.f, 0.f, 0.f));
    SetWidgetBoundsAndMaybeAnimateTransform(
        cannot_snap_widget_.get(),
        cannot_snap_widget_->GetBoundsCenteredIn(
            gfx::ToEnclosingRect(inset_bounds)),
        new_animation_type, nullptr);
  }

  translation_y_map_.clear();
  aura::Window::Windows windows = GetWindowsForHomeGesture();
  for (auto* window : windows) {
    // Cache the original y translation when setting bounds. They will be
    // possibly used later when swiping up from the shelf to close overview. Use
    // the target transform as some windows may still be animating.
    translation_y_map_[window] =
        window->layer()->GetTargetTransform().To2dTranslation().y();
  }
}

void OverviewItem::SendAccessibleSelectionEvent() {
  caption_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kSelection, true);
}

void OverviewItem::AnimateAndCloseWindow(bool up) {
  base::RecordAction(base::UserMetricsAction("WindowSelector_SwipeToClose"));

  animating_to_close_ = true;
  overview_session_->PositionWindows(/*animate=*/true);
  caption_container_view_->ResetEventDelegate();
  close_button_->ResetListener();

  int translation_y = kSwipeToCloseCloseTranslationDp * (up ? -1 : 1);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2d(0, translation_y));

  auto animate_window = [this](aura::Window* window,
                               const gfx::Transform& transform, bool observe) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM, window);
    gfx::Transform original_transform = window->transform();
    original_transform.ConcatTransform(transform);
    window->SetTransform(original_transform);
    if (observe)
      settings.AddObserver(this);
  };

  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);
  if (cannot_snap_widget_)
    animate_window(cannot_snap_widget_->GetNativeWindow(), transform, false);
  if (!transform_window_.IsMinimized())
    animate_window(GetWindow(), transform, false);
  animate_window(item_widget_->GetNativeWindow(), transform, true);
}

void OverviewItem::CloseWindow() {
  gfx::RectF inset_bounds(target_bounds_);
  inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                     target_bounds_.height() * kPreCloseScale);
  // Scale down both the window and label.
  SetBounds(inset_bounds, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);
  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);
  transform_window_.Close();
}

void OverviewItem::OnMinimizedStateChanged() {
  const bool minimized = transform_window_.IsMinimized();
  caption_container_view_->SetShowPreview(minimized);
  if (!minimized)
    EnsureVisible();
}

void OverviewItem::UpdateCannotSnapWarningVisibility() {
  // Windows which can snap will never show this warning. Or if the window is
  // the drop target window, also do not show this warning.
  bool visible = true;
  if (CanSnapInSplitview(GetWindow()) ||
      overview_grid_->IsDropTargetWindow(GetWindow())) {
    visible = false;
  } else {
    const SplitViewState state = Shell::Get()->split_view_controller()->state();
    visible = state == SplitViewState::kLeftSnapped ||
              state == SplitViewState::kRightSnapped;
  }

  if (!visible && !cannot_snap_widget_)
    return;

  if (!cannot_snap_widget_) {
    RoundedLabelWidget::InitParams params;
    params.horizontal_padding = kSplitviewLabelHorizontalInsetDp;
    params.vertical_padding = kSplitviewLabelVerticalInsetDp;
    params.background_color = kSplitviewLabelBackgroundColor;
    params.foreground_color = kSplitviewLabelEnabledColor;
    params.rounding_dp = kSplitviewLabelRoundRectRadiusDp;
    params.preferred_height = kSplitviewLabelPreferredHeightDp;
    params.message_id = IDS_ASH_SPLIT_VIEW_CANNOT_SNAP;
    params.parent =
        root_window()->GetChildById(kShellWindowId_AlwaysOnTopContainer);
    params.hide_in_mini_view = true;
    cannot_snap_widget_ = std::make_unique<RoundedLabelWidget>();
    cannot_snap_widget_->Init(std::move(params));
  }

  DoSplitviewOpacityAnimation(cannot_snap_widget_->GetNativeWindow()->layer(),
                              visible
                                  ? SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN
                                  : SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
  gfx::Rect bounds = gfx::ToEnclosingRect(target_bounds());
  bounds.Inset(kWindowMargin, kWindowMargin);
  bounds.Inset(gfx::Insets(kHeaderHeightDp, 0, 0, 0));
  cannot_snap_widget_->SetBoundsCenteredIn(bounds, /*animate=*/false);
}

void OverviewItem::OnSelectorItemDragStarted(OverviewItem* item) {
  is_being_dragged_ = (item == this);
  caption_container_view_->SetHeaderVisibility(
      is_being_dragged_
          ? CaptionContainerView::HeaderVisibility::kInvisible
          : CaptionContainerView::HeaderVisibility::kCloseButtonInvisibleOnly);
}

void OverviewItem::OnSelectorItemDragEnded(bool snap) {
  if (snap) {
    if (!is_being_dragged_)
      caption_container_view_->HideCloseInstantlyAndThenShowItSlowly();
  } else {
    caption_container_view_->SetHeaderVisibility(
        CaptionContainerView::HeaderVisibility::kVisible);
  }
  is_being_dragged_ = false;
}

ScopedOverviewTransformWindow::GridWindowFillMode
OverviewItem::GetWindowDimensionsType() const {
  return transform_window_.type();
}

void OverviewItem::UpdateWindowDimensionsType() {
  transform_window_.UpdateWindowDimensionsType();
  const bool show_backdrop =
      GetWindowDimensionsType() !=
      ScopedOverviewTransformWindow::GridWindowFillMode::kNormal;
  caption_container_view_->SetBackdropVisibility(show_backdrop);
}

gfx::Rect OverviewItem::GetBoundsOfSelectedItem() {
  gfx::RectF original_bounds = target_bounds();
  ScaleUpSelectedItem(OVERVIEW_ANIMATION_NONE);
  gfx::RectF selected_bounds = transform_window_.GetTransformedBounds();
  SetBounds(original_bounds, OVERVIEW_ANIMATION_NONE);
  return gfx::ToEnclosedRect(selected_bounds);
}

void OverviewItem::ScaleUpSelectedItem(OverviewAnimationType animation_type) {
  gfx::RectF scaled_bounds = target_bounds();
  scaled_bounds.Inset(-scaled_bounds.width() * kDragWindowScale,
                      -scaled_bounds.height() * kDragWindowScale);
  SetBounds(scaled_bounds, animation_type);
}

bool OverviewItem::IsDragItem() {
  return overview_session_->window_drag_controller() &&
         overview_session_->window_drag_controller()->item() == this;
}

void OverviewItem::OnDragAnimationCompleted() {
  // This is function is called whenever the grid repositions its windows, but
  // we only need to restack the windows if an item was being dragged around
  // and then released.
  if (!should_restack_on_animation_end_)
    return;

  should_restack_on_animation_end_ = false;

  // First stack this item's window below the snapped window if split view
  // mode is active.
  aura::Window* dragged_window = GetWindow();
  aura::Window* dragged_widget_window = item_widget_->GetNativeWindow();
  aura::Window* parent_window = dragged_widget_window->parent();
  if (Shell::Get()->split_view_controller()->InSplitViewMode()) {
    aura::Window* snapped_window =
        Shell::Get()->split_view_controller()->GetDefaultSnappedWindow();
    if (snapped_window->parent() == parent_window &&
        dragged_window->parent() == parent_window) {
      parent_window->StackChildBelow(dragged_window, snapped_window);
      parent_window->StackChildBelow(dragged_widget_window, dragged_window);
    }
  }

  // Then find the window which was stacked right above this overview item's
  // window before dragging and stack this overview item's window below it.
  const std::vector<std::unique_ptr<OverviewItem>>& overview_items =
      overview_grid_->window_list();
  aura::Window* stacking_target = nullptr;
  for (size_t index = 0; index < overview_items.size(); ++index) {
    if (index > 0) {
      aura::Window* window = overview_items[index - 1].get()->GetWindow();
      if (window->parent() == parent_window &&
          dragged_window->parent() == parent_window) {
        stacking_target =
            overview_items[index - 1].get()->item_widget()->GetNativeWindow();
      }
    }
    if (overview_items[index].get() == this && stacking_target) {
      parent_window->StackChildBelow(dragged_window, stacking_target);
      parent_window->StackChildBelow(dragged_widget_window, dragged_window);
      break;
    }
  }
}

void OverviewItem::UpdatePhantomsForDragging(bool is_touch_dragging) {
  DCHECK(AreMultiDisplayOverviewAndSplitViewEnabled());
  DCHECK_GT(Shell::GetAllRootWindows().size(), 1u);
  if (!phantoms_for_dragging_) {
    phantoms_for_dragging_ = std::make_unique<DragWindowController>(
        transform_window_.IsMinimized() ? item_widget_->GetNativeWindow()
                                        : GetWindow(),
        is_touch_dragging);
  }
  phantoms_for_dragging_->Update();
}

void OverviewItem::DestroyPhantomsForDragging() {
  DCHECK(AreMultiDisplayOverviewAndSplitViewEnabled());
  phantoms_for_dragging_.reset();
}

void OverviewItem::SetShadowBounds(base::Optional<gfx::Rect> bounds_in_screen) {
  // Shadow is normally turned off during animations and reapplied when they
  // are finished. On destruction, |shadow_| is cleaned up before
  // |transform_window_|, which may call this function, so early exit if
  // |shadow_| is nullptr.
  if (!shadow_)
    return;

  if (bounds_in_screen == base::nullopt) {
    shadow_->layer()->SetVisible(false);
    return;
  }

  shadow_->layer()->SetVisible(true);
  gfx::Rect bounds_in_item =
      gfx::Rect(item_widget_->GetNativeWindow()->GetTargetBounds().size());
  bounds_in_item.Inset(kOverviewMargin, kOverviewMargin);
  bounds_in_item.Inset(0, kHeaderHeightDp, 0, 0);
  bounds_in_item.ClampToCenteredSize(bounds_in_screen.value().size());

  shadow_->SetContentBounds(bounds_in_item);
}

void OverviewItem::UpdateRoundedCornersAndShadow() {
  // Do not show the rounded corners and the shadow if overview is shutting
  // down or we're currently in entering overview animation. Also don't update
  // or animate the window's frame header clip under these conditions.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool is_shutting_down =
      !overview_controller || !overview_controller->InOverviewSession();
  const bool should_show_rounded_corners =
      !disable_mask_ && !is_shutting_down &&
      !overview_controller->IsInStartAnimation();

  transform_window_.UpdateRoundedCorners(
      should_show_rounded_corners,
      /*update_clip=*/should_show_rounded_corners);

  // In addition, the shadow should be hidden if
  // 1) this overview item is the drop target window or
  // 2) this overview item is in animation.
  const bool should_show_shadow =
      should_show_rounded_corners &&
      !overview_grid_->IsDropTargetWindow(GetWindow()) &&
      !transform_window_.GetOverviewWindow()
           ->layer()
           ->GetAnimator()
           ->is_animating();

  SetShadowBounds(should_show_shadow
                      ? base::make_optional(gfx::ToEnclosedRect(
                            transform_window_.GetTransformedBounds()))
                      : base::nullopt);
  if (transform_window_.IsMinimized()) {
    caption_container_view_->UpdatePreviewRoundedCorners(
        should_show_rounded_corners,
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::EMPHASIS_LOW));
  }
}

void OverviewItem::OnStartingAnimationComplete() {
  DCHECK(item_widget_);
  if (transform_window_.IsMinimized()) {
    // Fade the title in if minimized. The rest of |item_widget_| should
    // already be shown.
    caption_container_view_->SetHeaderVisibility(
        CaptionContainerView::HeaderVisibility::kVisible);
  } else {
    FadeInWidgetAndMaybeSlideOnEnter(
        item_widget_.get(), OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
        /*slide=*/false, /*observe=*/false);
  }
  const bool show_backdrop =
      GetWindowDimensionsType() !=
      ScopedOverviewTransformWindow::GridWindowFillMode::kNormal;
  caption_container_view_->SetBackdropVisibility(show_backdrop);
  UpdateCannotSnapWarningVisibility();
}

void OverviewItem::StopWidgetAnimation() {
  DCHECK(item_widget_.get());
  item_widget_->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();
}

void OverviewItem::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  transform_window_.SetOpacity(opacity);
  if (cannot_snap_widget_)
    cannot_snap_widget_->SetOpacity(opacity);
}

float OverviewItem::GetOpacity() {
  return item_widget_->GetNativeWindow()->layer()->GetTargetOpacity();
}

OverviewAnimationType OverviewItem::GetExitOverviewAnimationType() {
  if (overview_session_->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT
             : OVERVIEW_ANIMATION_NONE;
}

OverviewAnimationType OverviewItem::GetExitTransformAnimationType() {
  if (overview_session_->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_ ? OVERVIEW_ANIMATION_RESTORE_WINDOW
                                      : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

void OverviewItem::HandleMouseEvent(const ui::MouseEvent& event) {
  const gfx::PointF screen_location = event.target()->GetScreenLocationF(event);
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      HandlePressEvent(screen_location, /*from_touch_gesture=*/false);
      break;
    case ui::ET_MOUSE_RELEASED:
      HandleReleaseEvent(screen_location);
      break;
    case ui::ET_MOUSE_DRAGGED:
      HandleDragEvent(screen_location);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void OverviewItem::HandleGestureEvent(ui::GestureEvent* event) {
  if (ShouldUseTabletModeGridLayout()) {
    HandleGestureEventForTabletModeLayout(event);
    return;
  }

  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      HandlePressEvent(location, /*from_touch_gesture=*/true);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      HandleDragEvent(location);
      break;
    case ui::ET_SCROLL_FLING_START:
      HandleFlingStartEvent(location, event->details().velocity_x(),
                            event->details().velocity_y());
      break;
    case ui::ET_GESTURE_SCROLL_END:
      HandleReleaseEvent(location);
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      HandleLongPressEvent(location);
      break;
    case ui::ET_GESTURE_TAP:
      HandleTapEvent();
      break;
    case ui::ET_GESTURE_END:
      HandleGestureEndEvent();
      break;
    default:
      break;
  }
}

void OverviewItem::HandleGestureEventForTabletModeLayout(
    ui::GestureEvent* event) {
  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
      if (IsDragItem()) {
        HandleFlingStartEvent(location, event->details().velocity_x(),
                              event->details().velocity_y());
      } else {
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      }
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (std::abs(event->details().scroll_y_hint()) >
          std::abs(event->details().scroll_x_hint())) {
        HandlePressEvent(location, /*from_touch_gesture=*/true);
      } else {
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (IsDragItem())
        HandleDragEvent(location);
      else
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      break;
    case ui::ET_GESTURE_SCROLL_END:
      if (IsDragItem())
        HandleReleaseEvent(location);
      else
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      HandlePressEvent(location, /*from_touch_gesture=*/true);
      HandleLongPressEvent(location);
      break;
    case ui::ET_GESTURE_TAP:
      overview_session_->SelectWindow(this);
      break;
    case ui::ET_GESTURE_END:
      HandleGestureEndEvent();
      break;
    default:
      overview_grid()->grid_event_handler()->OnGestureEvent(event);
      break;
  }
}

bool OverviewItem::ShouldIgnoreGestureEvents() {
  return IsSlidingOutOverviewFromShelf();
}

void OverviewItem::OnHighlightedViewActivated() {
  overview_session_->OnHighlightedItemActivated(this);
}

void OverviewItem::OnHighlightedViewClosed() {
  overview_session_->OnHighlightedItemClosed(this);
}

void OverviewItem::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  DCHECK_EQ(sender, close_button_);
  if (IsSlidingOutOverviewFromShelf())
    return;

  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
  }
  CloseWindow();
}

void OverviewItem::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  // Do not keep the overview bounds if we're shutting down.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION) {
    if (window == GetWindow()) {
      caption_container_view_->UpdatePreviewView();
    } else {
      // Transient window is repositioned. The new position within the
      // overview item needs to be recomputed.
      SetBounds(target_bounds_, OVERVIEW_ANIMATION_NONE);
    }
  }
}

void OverviewItem::OnWindowDestroying(aura::Window* window) {
  // Stops observing the window and all of its transient descendents.
  for (auto* window_iter : WindowTransientDescendantIteratorRange(
           WindowTransientDescendantIterator(window))) {
    window_iter->RemoveObserver(this);
  }
  if (window != GetWindow())
    return;
  transform_window_.OnWindowDestroyed();

  if (is_being_dragged_) {
    Shell::Get()->overview_controller()->UnpauseOcclusionTracker(
        kOcclusionPauseDurationForDragMs);
  }
}

void OverviewItem::OnWindowTitleChanged(aura::Window* window) {
  if (window != GetWindow())
    return;

  caption_container_view_->SetTitle(window->GetTitle());
}

void OverviewItem::OnImplicitAnimationsCompleted() {
  transform_window_.Close();
}

views::ImageButton* OverviewItem::GetCloseButtonForTesting() {
  return static_cast<views::ImageButton*>(close_button_);
}

float OverviewItem::GetCloseButtonVisibilityForTesting() const {
  return close_button_->layer()->opacity();
}

float OverviewItem::GetTitlebarOpacityForTesting() const {
  return caption_container_view_->header_view()->layer()->opacity();
}

gfx::Rect OverviewItem::GetShadowBoundsForTesting() {
  if (!shadow_ || !shadow_->layer()->visible())
    return gfx::Rect();

  return shadow_->content_bounds();
}

void OverviewItem::SetItemBounds(const gfx::RectF& target_bounds,
                                 OverviewAnimationType animation_type) {
  aura::Window* window = GetWindow();
  DCHECK(root_window_ == window->GetRootWindow());
  gfx::RectF screen_rect = gfx::RectF(GetTargetBoundsInScreen());

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::SizeF screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::SizeF(1.f, 1.f));
  screen_rect.set_size(screen_size);

  const int top_view_inset = transform_window_.GetTopInset();
  gfx::RectF overview_item_bounds =
      transform_window_.ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, kHeaderHeightDp);
  // Do not set transform for drop target, set bounds instead.
  if (overview_grid_->IsDropTargetWindow(window)) {
    window->SetBoundsInScreen(gfx::ToEnclosedRect(overview_item_bounds),
                              WindowState::Get(window)->GetDisplay());
    window->SetTransform(gfx::Transform());
    return;
  }

  gfx::Transform transform = ScopedOverviewTransformWindow::GetTransformForRect(
      screen_rect, overview_item_bounds);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  SetTransform(window, transform);
}

void OverviewItem::CreateWindowLabel() {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.visible_on_all_workspaces = true;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewModeLabel";
  params.activatable =
      views::Widget::InitParams::Activatable::ACTIVATABLE_DEFAULT;
  params.accept_events = true;
  params.parent = transform_window_.window()->parent();
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(std::move(params));
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window, GetWindow());

  shadow_ = std::make_unique<ui::Shadow>();
  shadow_->Init(kShadowElevation);
  item_widget_->GetLayer()->Add(shadow_->layer());

  close_button_ = new OverviewCloseButton(this);
  caption_container_view_ = new CaptionContainerView(
      this, GetWindow(), transform_window_.IsMinimized(), close_button_);
  item_widget_->SetContentsView(caption_container_view_);
  item_widget_->Show();
  item_widget_->SetOpacity(0.f);
  item_widget_->GetLayer()->SetMasksToBounds(false);
}

void OverviewItem::UpdateHeaderLayout(OverviewAnimationType animation_type) {
  gfx::RectF transformed_window_bounds =
      transform_window_.overview_bounds().value_or(
          transform_window_.GetTransformedBounds());
  ::wm::TranslateRectFromScreen(root_window_, &transformed_window_bounds);

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings(animation_type,
                                                     widget_window);
  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    animation_settings.AddObserver(enter_observer.get());
    Shell::Get()->overview_controller()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }

  // |widget_window| is sized to the same bounds as the original window plus
  // some space for the header and a little padding.
  gfx::Rect label_rect(0, -kHeaderHeightDp, transformed_window_bounds.width(),
                       kHeaderHeightDp + transformed_window_bounds.height());
  label_rect.Inset(-kOverviewMargin, -kOverviewMargin);
  widget_window->SetBounds(label_rect);

  gfx::Transform label_transform;
  label_transform.Translate(gfx::ToRoundedInt(transformed_window_bounds.x()),
                            gfx::ToRoundedInt(transformed_window_bounds.y()));
  widget_window->SetTransform(label_transform);
}

void OverviewItem::AnimateOpacity(float opacity,
                                  OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  ScopedOverviewAnimationSettings animation_settings_label(
      animation_type, item_widget_->GetNativeWindow());
  item_widget_->SetOpacity(opacity);

  if (cannot_snap_widget_) {
    aura::Window* cannot_snap_widget_window =
        cannot_snap_widget_->GetNativeWindow();
    ScopedOverviewAnimationSettings animation_settings_label(
        animation_type, cannot_snap_widget_window);
    cannot_snap_widget_window->layer()->SetOpacity(opacity);
  }
}

void OverviewItem::HandlePressEvent(const gfx::PointF& location_in_screen,
                                    bool from_touch_gesture) {
  // We allow switching finger while dragging, but do not allow dragging two
  // or more items.
  if (overview_session_->window_drag_controller() &&
      overview_session_->window_drag_controller()->item()) {
    return;
  }

  StartDrag();
  overview_session_->InitiateDrag(this, location_in_screen,
                                  /*is_touch_dragging=*/from_touch_gesture);
}

void OverviewItem::HandleReleaseEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  overview_session_->CompleteDrag(this, location_in_screen);
}

void OverviewItem::HandleDragEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  overview_session_->Drag(this, location_in_screen);
}

void OverviewItem::HandleLongPressEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  if (ShouldAllowSplitView() || desks_util::ShouldDesksBarBeCreated())
    overview_session_->StartNormalDragMode(location_in_screen);
}

void OverviewItem::HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                                         float velocity_x,
                                         float velocity_y) {
  overview_session_->Fling(this, location_in_screen, velocity_x, velocity_y);
}

void OverviewItem::HandleTapEvent() {
  if (!IsDragItem())
    return;

  overview_session_->ActivateDraggedWindow();
}

void OverviewItem::HandleGestureEndEvent() {
  if (!IsDragItem())
    return;

  overview_session_->ResetDraggedWindowGesture();
}

void OverviewItem::StartDrag() {
  // |transform_window_| handles hiding shadow and rounded edges mask while
  // animating, and applies them after animation is complete. Prevent the
  // shadow and rounded edges mask from showing up after dragging in the case
  // the window is pressed while still animating.
  transform_window_.CancelAnimationsListener();

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  aura::Window* window = GetWindow();
  if (widget_window && widget_window->parent() == window->parent()) {
    // TODO(xdai): This might not work if there is an always on top window.
    // See crbug.com/733760.
    widget_window->parent()->StackChildAtTop(window);
    widget_window->parent()->StackChildBelow(widget_window, window);
  }
}

aura::Window::Windows OverviewItem::GetWindowsForHomeGesture() {
  aura::Window::Windows windows = {item_widget_->GetNativeWindow()};
  if (!transform_window_.IsMinimized()) {
    for (auto* window : GetTransientTreeIterator(GetWindow()))
      windows.push_back(window);
  }
  if (cannot_snap_widget_)
    windows.push_back(cannot_snap_widget_->GetNativeWindow());
  return windows;
}

}  // namespace ash
