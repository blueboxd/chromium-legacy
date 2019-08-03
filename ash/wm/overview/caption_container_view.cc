// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/caption_container_view.h"

#include <memory>

#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/window_preview_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Foreground label color.
constexpr SkColor kLabelColor = SK_ColorWHITE;

// Horizontal padding for the label, on both sides.
constexpr int kHorizontalLabelPaddingDp = 12;

// The size in dp of the window icon shown on the overview window next to the
// title.
constexpr gfx::Size kIconSize{24, 24};

// The font delta of the window title.
constexpr int kLabelFontDelta = 2;

// Values of the backdrop.
constexpr int kBackdropRoundingDp = 4;
constexpr SkColor kBackdropColor = SkColorSetA(SK_ColorWHITE, 0x24);

constexpr int kHeaderPreferredHeightDp = 40;
constexpr int kMarginDp = 10;

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

gfx::PointF ConvertToScreen(views::View* view, const gfx::Point& location) {
  gfx::Point location_copy(location);
  views::View::ConvertPointToScreen(view, &location_copy);
  return gfx::PointF(location_copy);
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

CaptionContainerView::CaptionContainerView(EventDelegate* event_delegate,
                                           aura::Window* window,
                                           bool show_preview,
                                           views::ImageButton* close_button)
    : views::Button(nullptr),
      event_delegate_(event_delegate),
      window_(window),
      close_button_(close_button) {
  // This should not be focusable. It's also to avoid accessibility error when
  // |window->GetTitle()| is empty.
  SetFocusBehavior(FocusBehavior::NEVER);
  SetAccessibleName(window->GetTitle());

  header_view_ = new views::View();
  views::BoxLayout* layout =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kHorizontalLabelPaddingDp));
  AddChildWithLayer(this, header_view_);

  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically larger.
  gfx::ImageSkia* icon = window->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty())
    icon = window->GetProperty(aura::client::kWindowIconKey);
  if (icon && !icon->size().IsEmpty()) {
    image_view_ = new views::ImageView();
    image_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
    image_view_->SetSize(kIconSize);
    header_view_->AddChildView(image_view_);
  }

  title_label_ = new views::Label(window->GetTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColor(kLabelColor);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  header_view_->AddChildView(title_label_);
  layout->SetFlexForView(title_label_, 1);

  if (close_button)
    AddChildWithLayer(header_view_, close_button);

  // Call this last as it calls |Layout()| which relies on the some of the other
  // elements existing.
  SetShowPreview(show_preview);
  if (show_preview) {
    header_view_->layer()->SetOpacity(0.f);
    current_header_visibility_ = HeaderVisibility::kInvisible;
  }
}

CaptionContainerView::~CaptionContainerView() = default;

void CaptionContainerView::SetHeaderVisibility(HeaderVisibility visibility) {
  DCHECK(header_view_->layer());
  if (visibility == current_header_visibility_)
    return;
  const HeaderVisibility previous_visibility = current_header_visibility_;
  current_header_visibility_ = visibility;

  const bool all_invisible = visibility == HeaderVisibility::kInvisible;
  AnimateLayerOpacity(header_view_->layer(), !all_invisible);

  // If |header_view_| is fading out, we are done. Depending on if the close
  // button was visible, it will fade out with |header_view_| or stay hidden.
  if (all_invisible || !close_button_)
    return;

  const bool close_button_visible = visibility == HeaderVisibility::kVisible;
  // If |header_view_| was hidden and is fading in, set the opacity of
  // |close_button_| depending on whether the close button should fade in with
  // |header_view_| or stay hidden.
  if (previous_visibility == HeaderVisibility::kInvisible) {
    close_button_->layer()->SetOpacity(close_button_visible ? 1.f : 0.f);
    return;
  }

  AnimateLayerOpacity(close_button_->layer(), close_button_visible);
}

void CaptionContainerView::HideCloseInstantlyAndThenShowItSlowly() {
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

void CaptionContainerView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible)
    return;

  if (!backdrop_view_) {
    backdrop_view_ = new RoundedRectView(kBackdropRoundingDp, kBackdropColor);
    backdrop_view_->set_can_process_events_within_subtree(false);
    AddChildWithLayer(this, backdrop_view_);
    Layout();
  }
  backdrop_view_->SetVisible(visible);
}

void CaptionContainerView::ResetEventDelegate() {
  event_delegate_ = nullptr;
}

void CaptionContainerView::SetTitle(const base::string16& title) {
  title_label_->SetText(title);
  SetAccessibleName(title);
}

void CaptionContainerView::SetShowPreview(bool show) {
  if (show == !!preview_view_)
    return;

  if (!show) {
    RemoveChildView(preview_view_);
    preview_view_ = nullptr;
    return;
  }

  preview_view_ = new WindowPreviewView(window_, false);
  AddChildWithLayer(this, preview_view_);
  Layout();
}

void CaptionContainerView::UpdatePreviewRoundedCorners(bool show,
                                                       float rounding) {
  if (!preview_view_)
    return;

  const float scale = preview_view_->layer()->transform().Scale2d().x();
  const gfx::RoundedCornersF radii(show ? rounding / scale : 0.0f);
  preview_view_->layer()->SetRoundedCornerRadius(radii);
  preview_view_->layer()->SetIsFastRoundedCorner(true);
}

void CaptionContainerView::UpdatePreviewView() {
  if (!preview_view_)
    return;

  preview_view_->RecreatePreviews();
  Layout();
}

views::View* CaptionContainerView::GetView() {
  return this;
}

gfx::Rect CaptionContainerView::GetHighlightBoundsInScreen() {
  // Use the target bounds instead of |GetBoundsInScreen()| because |this| may
  // be animating. However, the origin will be incorrect because the windows are
  // always positioned above and left of the parents origin, then translated. To
  // get the proper origin we use |GetBoundsInScreen()| which takes into account
  // the transform (but returns the wrong height and width).
  auto* window = GetWidget()->GetNativeWindow();
  gfx::Rect target_bounds = window->GetTargetBounds();
  target_bounds.set_origin(window->GetBoundsInScreen().origin());
  return target_bounds;
}

void CaptionContainerView::MaybeActivateHighlightedView() {
  if (event_delegate_)
    event_delegate_->OnHighlightedViewActivated();
}

void CaptionContainerView::MaybeCloseHighlightedView() {
  if (event_delegate_)
    event_delegate_->OnHighlightedViewClosed();
}

void CaptionContainerView::Layout() {
  gfx::Rect bounds(GetLocalBounds());
  bounds.Inset(kMarginDp, kMarginDp);

  if (backdrop_view_) {
    gfx::Rect backdrop_bounds = bounds;
    backdrop_bounds.Inset(0, kHeaderPreferredHeightDp, 0, 0);
    backdrop_view_->SetBoundsRect(backdrop_bounds);
  }

  if (preview_view_) {
    gfx::Rect preview_bounds = bounds;
    preview_bounds.Inset(0, kHeaderPreferredHeightDp, 0, 0);
    preview_bounds.ClampToCenteredSize(preview_view_->CalculatePreferredSize());
    preview_view_->SetBoundsRect(preview_bounds);
  }

  // Position the header at the top.
  const gfx::Rect header_bounds(kMarginDp, kMarginDp,
                                GetLocalBounds().width() - kMarginDp,
                                kHeaderPreferredHeightDp);
  header_view_->SetBoundsRect(header_bounds);
}

const char* CaptionContainerView::GetClassName() const {
  return "CaptionContainerView";
}

bool CaptionContainerView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event_delegate_)
    return Button::OnMousePressed(event);
  event_delegate_->HandlePressEvent(ConvertToScreen(this, event.location()),
                                    /*from_touch_gesture=*/false);
  return true;
}

bool CaptionContainerView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event_delegate_)
    return Button::OnMouseDragged(event);
  event_delegate_->HandleDragEvent(ConvertToScreen(this, event.location()));
  return true;
}

void CaptionContainerView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event_delegate_) {
    Button::OnMouseReleased(event);
    return;
  }
  event_delegate_->HandleReleaseEvent(ConvertToScreen(this, event.location()));
}

void CaptionContainerView::OnGestureEvent(ui::GestureEvent* event) {
  if (!event_delegate_)
    return;

  if (event_delegate_->ShouldIgnoreGestureEvents()) {
    event->SetHandled();
    return;
  }

  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      event_delegate_->HandlePressEvent(location, /*from_touch_gesture=*/true);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      event_delegate_->HandleDragEvent(location);
      break;
    case ui::ET_SCROLL_FLING_START:
      event_delegate_->HandleFlingStartEvent(location,
                                             event->details().velocity_x(),
                                             event->details().velocity_y());
      break;
    case ui::ET_GESTURE_SCROLL_END:
      event_delegate_->HandleReleaseEvent(location);
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      event_delegate_->HandleLongPressEvent(location);
      break;
    case ui::ET_GESTURE_TAP:
      event_delegate_->HandleTapEvent();
      break;
    case ui::ET_GESTURE_END:
      event_delegate_->HandleGestureEndEvent();
      break;
    default:
      break;
  }
  event->SetHandled();
}

bool CaptionContainerView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::ET_GESTURE_TAP_DOWN,
                                        ui::ET_MOUSE_PRESSED};
  if (event.IsLocatedEvent() && base::Contains(press_types, event.type())) {
    gfx::Rect inset_bounds = GetLocalBounds();
    inset_bounds.Inset(gfx::Insets(kMarginDp));
    if (!inset_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && Button::CanAcceptEvent(event);
}

}  // namespace ash
