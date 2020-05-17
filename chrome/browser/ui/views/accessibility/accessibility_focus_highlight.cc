// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/accessibility_focus_highlight.h"

#include "base/numerics/ranges.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace ui {
class Compositor;
}

namespace {

// The padding between the bounds of the layer and the bounds of the
// drawn focus ring, in DIPs. If it's zero the focus ring might be
// clipped.
constexpr int kLayerPaddingDIPs = 2;

// The number of pixels the focus ring is outset from the object it outlines,
// which also determines the border radius of the rounded corners.
constexpr int kAccessibilityFocusHighlightMarginDIPs = 7;

// The stroke width, in DIPs, of each line drawn as part of the focus ring
// gradient effect.
constexpr int kStrokeWidthDIPs = 2;

// The thickness, in DIPs, of the focus ring gradient.
constexpr int kGradientWidthDIPs = 3;

// The amount of time it should take for the highlight to fade in.
constexpr int kFadeInTimeMilliseconds = 100;

// The amount of time it should take for the highlight to fade out.
constexpr int kFadeOutTimeMilliseconds = 1600;

}  // namespace

// static
SkColor AccessibilityFocusHighlight::color_;

// static
base::TimeDelta AccessibilityFocusHighlight::fade_in_time_;

// static
base::TimeDelta AccessibilityFocusHighlight::fade_out_time_;

// static
bool AccessibilityFocusHighlight::skip_activation_check_for_testing_ = false;

AccessibilityFocusHighlight::AccessibilityFocusHighlight(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      device_scale_factor_(
          browser_view_->GetWidget()->GetLayer()->device_scale_factor()) {
  DCHECK(browser_view);

  // Listen for focus changes. Automatically deregisters when destroyed.
  notification_registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                              content::NotificationService::AllSources());

  // One-time initialization of statics the first time an instance is created.
  if (fade_in_time_.is_zero()) {
    fade_in_time_ = base::TimeDelta::FromMilliseconds(kFadeInTimeMilliseconds);
    fade_out_time_ =
        base::TimeDelta::FromMilliseconds(kFadeOutTimeMilliseconds);
    color_ = SkColorSetRGB(247, 152, 58);
  }
}

AccessibilityFocusHighlight::~AccessibilityFocusHighlight() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);
}

// static
void AccessibilityFocusHighlight::SetNoFadeForTesting() {
  fade_in_time_ = base::TimeDelta();
  fade_out_time_ = base::TimeDelta::FromHours(1);
}

// static
void AccessibilityFocusHighlight::SkipActivationCheckForTesting() {
  skip_activation_check_for_testing_ = true;
}

// static
SkColor AccessibilityFocusHighlight::GetHighlightColorForTesting() {
  return color_;
}

void AccessibilityFocusHighlight::CreateOrUpdateLayer() {
  // Find the layer of our owning BrowserView.
  views::Widget* widget = browser_view_->GetWidget();
  DCHECK(widget);
  ui::Layer* root_layer = widget->GetLayer();

  // Create the layer if needed.
  if (!layer_) {
    layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
    layer_->SetName("AccessibilityFocusHighlight");
    layer_->SetFillsBoundsOpaquely(false);
    root_layer->Add(layer_.get());
    // Initially transparent so it can fade in.
    layer_->SetOpacity(0.0f);
    layer_->set_delegate(this);
    layer_created_time_ = base::TimeTicks::Now();
  }

  // Each time this is called, move it to the top in case new layers
  // have been added since we created this layer.
  layer_->parent()->StackAtTop(layer_.get());

  // Update the bounds.
  layer_->SetBounds(bounds_);

  // Update the timestamp of the last time the layer changed.
  focus_last_changed_time_ = base::TimeTicks::Now();

  // Ensure it's repainted.
  gfx::Rect layer_bounds(0, 0, bounds_.width(), bounds_.height());
  layer_->SchedulePaint(layer_bounds);

  // Schedule the animation observer, or update it if needed.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds_);
  ui::Compositor* compositor = root_layer->GetCompositor();
  if (compositor != compositor_) {
    if (compositor_ && compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = compositor;
    if (compositor_ && !compositor_->HasAnimationObserver(this))
      compositor_->AddAnimationObserver(this);
  }
}

void AccessibilityFocusHighlight::RemoveLayer() {
  layer_.reset();
  if (compositor_) {
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

void AccessibilityFocusHighlight::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE)
    return;

  // Unless this is a test, only draw the focus ring if this BrowserView is
  // the active one.
  if (!browser_view_->IsActive() && !skip_activation_check_for_testing_)
    return;

  // Get the bounds of the focused node from the web page. Initially it's
  // given to us in screen DIPs.
  content::FocusedNodeDetails* node_details =
      content::Details<content::FocusedNodeDetails>(details).ptr();
  bounds_ = node_details->node_bounds_in_screen;

  // Convert it to the local coordinates of this BrowserView's widget.
  bounds_.Offset(-gfx::ToFlooredVector2d(browser_view_->GetWidget()
                                             ->GetClientAreaBoundsInScreen()
                                             .OffsetFromOrigin()));

  // Outset the bounds by the margin of the focus ring plus the layer padding.
  int outset =
      int{(kAccessibilityFocusHighlightMarginDIPs + kLayerPaddingDIPs) *
          device_scale_factor_};
  bounds_.Inset(-outset, -outset);

  // Create the layer if needed, and move/resize it.
  CreateOrUpdateLayer();
}

void AccessibilityFocusHighlight::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer_->size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  float dsf = device_scale_factor_;

  flags.setStrokeWidth(kStrokeWidthDIPs * dsf);

  int padding = int{kLayerPaddingDIPs * dsf};
  int width = bounds_.width() - 2 * padding;
  int height = bounds_.height() - 2 * padding;
  int margin = int{kAccessibilityFocusHighlightMarginDIPs * dsf};

  // Translate the coordinate space so that we can draw the rounded
  // rect at (0, 0) and not need to take the padding into account.
  recorder.canvas()->Translate(gfx::Vector2d(padding, padding));

  // Create a gradient effect by drawing the path outline multiple
  // times with increasing insets from 0 to kGradientWidthDIPs, and
  // with increasing transparency.
  int w = int{kGradientWidthDIPs * dsf};
  for (int i = 0; i < w; ++i) {
    // Distance remaining within border gradient.
    int dist = w - i;
    // Decrease alpha as distance remaining decreases.
    int alpha = 255 * dist * dist / (w * w);
    flags.setColor(SkColorSetA(color_, alpha));

    gfx::RectF rect(i, i, width - i, height - i);
    recorder.canvas()->DrawRoundRect(rect, margin - i, flags);
  }
}

void AccessibilityFocusHighlight::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  // The layer will automatically be invalildated, we don't need to do it
  // explicitly.
  device_scale_factor_ = new_device_scale_factor;
}

void AccessibilityFocusHighlight::OnAnimationStep(base::TimeTicks timestamp) {
  if (!layer_)
    return;

  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // focus change, so we just treat those as a delta of zero.
  if (timestamp < layer_created_time_)
    timestamp = layer_created_time_;

  // The time since the layer was created is used for fading in.
  base::TimeDelta time_since_layer_create = timestamp - layer_created_time_;

  // For fading out, we look at the time since focus last moved,
  // but we adjust it so that this "clock" doesn't start until after
  // the first fade in completes.
  base::TimeDelta time_since_focus_move =
      std::min(timestamp - focus_last_changed_time_,
               timestamp - layer_created_time_ - fade_in_time_);

  // If the fade out has completed, remove the layer and remove the
  // animation observer.
  if (time_since_focus_move > fade_out_time_) {
    RemoveLayer();
    return;
  }

  // Compute the opacity based on the fade in and fade out times.
  float opacity;
  if (time_since_layer_create < fade_in_time_) {
    // We're fading in.
    opacity = time_since_layer_create.InSecondsF() / fade_in_time_.InSecondsF();
  } else {
    // Fading out. Add fade_in_time_ and fade_out_time_ because we don't
    // want to start the fade out until after the fade in has finished.
    opacity = 1.0f - (time_since_focus_move.InSecondsF() /
                      fade_out_time_.InSecondsF());
  }

  // Layer::SetOpacity will throw an error if we're not within 0...1.
  opacity = base::ClampToRange(opacity, 0.0f, 1.0f);

  layer_->SetOpacity(opacity);
}

void AccessibilityFocusHighlight::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK(compositor);
  DCHECK_EQ(compositor, compositor_);
  if (compositor == compositor_) {
    compositor->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}
