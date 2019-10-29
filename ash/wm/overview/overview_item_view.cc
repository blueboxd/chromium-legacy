// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/window_preview_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Duration of the show/hide animation of the header.
constexpr base::TimeDelta kHeaderFadeDuration =
    base::TimeDelta::FromMilliseconds(167);

// Delay before the show animation of the header.
constexpr base::TimeDelta kHeaderFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);

// Duration of the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDuration =
    base::TimeDelta::FromMilliseconds(300);

// Delay before the slow show animation of the close button.
constexpr base::TimeDelta kCloseButtonSlowFadeInDelay =
    base::TimeDelta::FromMilliseconds(750);

void AddChildWithLayer(views::View* parent, views::View* child) {
  child->SetPaintToLayer();
  child->layer()->SetFillsBoundsOpaquely(false);
  parent->AddChildView(child);
}

// Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
// otherwise. The tween type differs for |visible| and if |visible| is true
// there is a slight delay before the animation begins. Does not animate if
// opacity matches |visible|.
void AnimateLayerOpacity(ui::Layer* layer, bool visible) {
  float target_opacity = visible ? 1.f : 0.f;
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  layer->SetOpacity(1.f - target_opacity);
  gfx::Tween::Type tween =
      visible ? gfx::Tween::LINEAR_OUT_SLOW_IN : gfx::Tween::FAST_OUT_LINEAR_IN;
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kHeaderFadeDuration);
  settings.SetTweenType(tween);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  if (visible) {
    layer->GetAnimator()->SchedulePauseForProperties(
        kHeaderFadeInDelay, ui::LayerAnimationElement::OPACITY);
  }
  layer->SetOpacity(target_opacity);
}

}  // namespace

OverviewItemView::OverviewItemView(EventDelegate* event_delegate,
                                   aura::Window* window,
                                   bool show_preview,
                                   views::ImageButton* close_button)
    : WindowMiniView(window),
      event_delegate_(event_delegate),
      close_button_(close_button) {
  // This should not be focusable. It's also to avoid accessibility error when
  // |window->GetTitle()| is empty.
  SetFocusBehavior(FocusBehavior::NEVER);

  if (close_button)
    AddChildWithLayer(header_view(), close_button);

  // Call this last as it calls |Layout()| which relies on the some of the other
  // elements existing.
  SetShowPreview(show_preview);
  if (show_preview) {
    header_view()->layer()->SetOpacity(0.f);
    current_header_visibility_ = HeaderVisibility::kInvisible;
  }
}

OverviewItemView::~OverviewItemView() = default;

void OverviewItemView::SetHeaderVisibility(HeaderVisibility visibility) {
  DCHECK(header_view()->layer());
  if (visibility == current_header_visibility_)
    return;
  const HeaderVisibility previous_visibility = current_header_visibility_;
  current_header_visibility_ = visibility;

  const bool all_invisible = visibility == HeaderVisibility::kInvisible;
  AnimateLayerOpacity(header_view()->layer(), !all_invisible);

  // If |header_view()| is fading out, we are done. Depending on if the close
  // button was visible, it will fade out with |header_view()| or stay hidden.
  if (all_invisible || !close_button_)
    return;

  const bool close_button_visible = visibility == HeaderVisibility::kVisible;
  // If |header_view()| was hidden and is fading in, set the opacity of
  // |close_button_| depending on whether the close button should fade in with
  // |header_view()| or stay hidden.
  if (previous_visibility == HeaderVisibility::kInvisible) {
    close_button_->layer()->SetOpacity(close_button_visible ? 1.f : 0.f);
    return;
  }

  AnimateLayerOpacity(close_button_->layer(), close_button_visible);
}

void OverviewItemView::HideCloseInstantlyAndThenShowItSlowly() {
  DCHECK(close_button_);
  DCHECK_NE(HeaderVisibility::kInvisible, current_header_visibility_);
  ui::Layer* layer = close_button_->layer();
  DCHECK(layer);
  current_header_visibility_ = HeaderVisibility::kVisible;
  layer->SetOpacity(0.f);
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(kCloseButtonSlowFadeInDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  layer->GetAnimator()->SchedulePauseForProperties(
      kCloseButtonSlowFadeInDelay, ui::LayerAnimationElement::OPACITY);
  layer->SetOpacity(1.f);
}

void OverviewItemView::ResetEventDelegate() {
  event_delegate_ = nullptr;
}

void OverviewItemView::RefreshPreviewView() {
  if (!preview_view())
    return;

  preview_view()->RecreatePreviews();
  Layout();
}

views::View* OverviewItemView::GetView() {
  return this;
}

gfx::Rect OverviewItemView::GetHighlightBoundsInScreen() {
  // Use the target bounds instead of |GetBoundsInScreen()| because |this| may
  // be animating. However, the origin will be incorrect because the windows are
  // always positioned above and left of the parents origin, then translated. To
  // get the proper origin we use |GetBoundsInScreen()| which takes into account
  // the transform (but returns the wrong height and width).
  auto* window = GetWidget()->GetNativeWindow();
  gfx::Rect target_bounds = window->GetTargetBounds();
  target_bounds.set_origin(window->GetBoundsInScreen().origin());
  target_bounds.Inset(kWindowMargin, kWindowMargin);
  return target_bounds;
}

void OverviewItemView::MaybeActivateHighlightedView() {
  if (event_delegate_)
    event_delegate_->OnHighlightedViewActivated();
}

void OverviewItemView::MaybeCloseHighlightedView() {
  if (event_delegate_)
    event_delegate_->OnHighlightedViewClosed();
}

gfx::Point OverviewItemView::GetMagnifierFocusPointInScreen() {
  // When this item is tabbed into, put the magnifier focus on the front of the
  // title, so that users can read the title first thing.
  const gfx::Rect title_bounds = title_label()->GetBoundsInScreen();
  return gfx::Point(GetMirroredXInView(title_bounds.x()),
                    title_bounds.CenterPoint().y());
}

const char* OverviewItemView::GetClassName() const {
  return "OverviewItemView";
}

bool OverviewItemView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event_delegate_)
    return Button::OnMousePressed(event);
  event_delegate_->HandleMouseEvent(event);
  return true;
}

bool OverviewItemView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event_delegate_)
    return Button::OnMouseDragged(event);
  event_delegate_->HandleMouseEvent(event);
  return true;
}

void OverviewItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event_delegate_) {
    Button::OnMouseReleased(event);
    return;
  }
  event_delegate_->HandleMouseEvent(event);
}

void OverviewItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (!event_delegate_)
    return;

  if (event_delegate_->ShouldIgnoreGestureEvents()) {
    event->SetHandled();
    return;
  }

  event_delegate_->HandleGestureEvent(event);
  event->SetHandled();
}

bool OverviewItemView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::ET_GESTURE_TAP_DOWN,
                                        ui::ET_MOUSE_PRESSED};
  if (event.IsLocatedEvent() && base::Contains(press_types, event.type())) {
    gfx::Rect inset_bounds = GetLocalBounds();
    inset_bounds.Inset(gfx::Insets(kOverviewMargin));
    if (!inset_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && Button::CanAcceptEvent(event);
}

void OverviewItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
}

}  // namespace ash
