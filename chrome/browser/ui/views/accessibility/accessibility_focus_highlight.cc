// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/accessibility_focus_highlight.h"

#include "base/numerics/ranges.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
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

// The number of pixels of padding between the outer edge of the focused
// element's bounding box and the inner edge of the inner focus ring.
constexpr int kPaddingDIPs = 8;

// The size of the border radius of the innermost focus highlight ring.
constexpr int kBorderRadiusDIPs = 4;

// The stroke width, in DIPs, of the innermost focus ring, and each line drawn
// as part of the focus ring gradient effect.
constexpr int kStrokeWidthDIPs = 2;

// The thickness, in DIPs, of the outer focus ring gradient.
constexpr int kGradientWidthDIPs = 9;

// The padding between the bounds of the layer and the bounds of the
// drawn focus ring, in DIPs. If it's zero the focus ring might be
// clipped.
constexpr int kLayerPaddingDIPs = 2;

// Total DIPs between the edge of the node and the edge of the layer.
constexpr int kTotalLayerPaddingDIPs =
    kPaddingDIPs + kStrokeWidthDIPs + kGradientWidthDIPs + kLayerPaddingDIPs;

// The amount of time it should take for the highlight to fade in.
constexpr int kFadeInTimeMilliseconds = 100;

// The amount of time the highlight should persist before beginning to fade.
constexpr int kHighlightPersistTimeMilliseconds = 1000;

// The amount of time it should take for the highlight to fade out.
constexpr int kFadeOutTimeMilliseconds = 600;

}  // namespace

// static
SkColor AccessibilityFocusHighlight::default_color_;

// static
base::TimeDelta AccessibilityFocusHighlight::fade_in_time_;

// static
base::TimeDelta AccessibilityFocusHighlight::persist_time_;

// static
base::TimeDelta AccessibilityFocusHighlight::fade_out_time_;

// static
bool AccessibilityFocusHighlight::skip_activation_check_for_testing_ = false;

// static
bool AccessibilityFocusHighlight::use_default_color_for_testing_ = false;

AccessibilityFocusHighlight::AccessibilityFocusHighlight(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      device_scale_factor_(
          browser_view_->GetWidget()->GetLayer()->device_scale_factor()) {
  DCHECK(browser_view);

  // Listen for preference changes.
  profile_pref_registrar_.Init(browser_view_->browser()->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kAccessibilityFocusHighlightEnabled,
      base::BindRepeating(
          &AccessibilityFocusHighlight::AddOrRemoveFocusObserver,
          base::Unretained(this)));

  // Initialise focus observer based on current preferences.
  AddOrRemoveFocusObserver();

  // One-time initialization of statics the first time an instance is created.
  if (fade_in_time_.is_zero()) {
    fade_in_time_ = base::TimeDelta::FromMilliseconds(kFadeInTimeMilliseconds);
    persist_time_ =
        base::TimeDelta::FromMilliseconds(kHighlightPersistTimeMilliseconds);
    fade_out_time_ =
        base::TimeDelta::FromMilliseconds(kFadeOutTimeMilliseconds);
    default_color_ = SkColorSetRGB(16, 16, 16);  // #101010
  }
}

AccessibilityFocusHighlight::~AccessibilityFocusHighlight() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);
}

// static
void AccessibilityFocusHighlight::SetNoFadeForTesting() {
  fade_in_time_ = base::TimeDelta();
  persist_time_ = base::TimeDelta::FromHours(1);
  fade_out_time_ = base::TimeDelta();
}

// static
void AccessibilityFocusHighlight::SkipActivationCheckForTesting() {
  skip_activation_check_for_testing_ = true;
}

// static
void AccessibilityFocusHighlight::UseDefaultColorForTesting() {
  use_default_color_for_testing_ = true;
}

SkColor AccessibilityFocusHighlight::GetHighlightColor() {
  SkColor theme_color = browser_view_->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_FocusedBorderColor);

  if (theme_color == SK_ColorTRANSPARENT || use_default_color_for_testing_)
    return default_color_;

  return theme_color;
}

void AccessibilityFocusHighlight::CreateOrUpdateLayer(gfx::Rect node_bounds) {
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
  // Outset the bounds of the layer by the total width of the focus highlight,
  // plus the extra padding to ensure the highlight isn't clipped.
  gfx::Rect layer_bounds = node_bounds;
  int padding = kTotalLayerPaddingDIPs * device_scale_factor_;
  layer_bounds.Inset(-padding, -padding);

  layer_->SetBounds(layer_bounds);

  // Set node_bounds_ and make their position relative to the layer, instead of
  // the page.
  node_bounds_ = node_bounds;
  node_bounds_.set_x(padding);
  node_bounds_.set_y(padding);

  // Update the timestamp of the last time the layer changed.
  focus_last_changed_time_ = base::TimeTicks::Now();

  // Ensure it's repainted.
  gfx::Rect bounds(0, 0, layer_bounds.width(), layer_bounds.height());
  layer_->SchedulePaint(bounds);

  // Schedule the animation observer, or update it if needed.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(layer_bounds);
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

void AccessibilityFocusHighlight::AddOrRemoveFocusObserver() {
  PrefService* prefs = browser_view_->browser()->profile()->GetPrefs();

  if (prefs->GetBoolean(prefs::kAccessibilityFocusHighlightEnabled)) {
    // Listen for focus changes. Automatically deregisters when destroyed,
    // or when the preference toggles off.
    notification_registrar_.Add(this,
                                content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                                content::NotificationService::AllSources());
    return;
  }

  if (notification_registrar_.IsRegistered(
          this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
          content::NotificationService::AllSources())) {
    notification_registrar_.Remove(this,
                                   content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                                   content::NotificationService::AllSources());
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
  gfx::Rect node_bounds = node_details->node_bounds_in_screen;

  // Convert it to the local coordinates of this BrowserView's widget.
  node_bounds.Offset(-gfx::ToFlooredVector2d(browser_view_->GetWidget()
                                                 ->GetClientAreaBoundsInScreen()
                                                 .OffsetFromOrigin()));

  // Create the layer if needed, and move/resize it.
  CreateOrUpdateLayer(node_bounds);
}

void AccessibilityFocusHighlight::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer_->size());
  SkColor highlight_color = GetHighlightColor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(highlight_color);

  gfx::RectF bounds(node_bounds_);

  // Draw gradient first, so other lines will be drawn over the top.
  // Apply padding
  int padding = kPaddingDIPs * device_scale_factor_;
  bounds.Inset(-padding, -padding);

  gfx::RectF gradient_bounds(bounds);
  int border_radius = kBorderRadiusDIPs * device_scale_factor_;
  int gradient_border_radius = border_radius;

  // Create a gradient effect by drawing the path outline multiple
  // times with increasing insets from 0 to kGradientWidthDIPs, and
  // with increasing transparency.
  int gradient_width = kGradientWidthDIPs * device_scale_factor_;
  int stroke_width = kStrokeWidthDIPs * device_scale_factor_;
  flags.setStrokeWidth(stroke_width);
  int original_alpha = std::min(SkColorGetA(highlight_color), 192u);
  for (int remaining = gradient_width; remaining > 0; remaining -= 1) {
    // Decrease alpha as distance remaining decreases.
    int alpha = (original_alpha * remaining * remaining) /
                (gradient_width * gradient_width);
    flags.setAlpha(alpha);

    recorder.canvas()->DrawRoundRect(gradient_bounds, gradient_border_radius,
                                     flags);

    gradient_bounds.Inset(-1, -1);
    gradient_border_radius += 1;
  }

  // Draw the white ring before the inner ring, so that the inner ring is
  // partially over the top, rather than drawing a 1px white ring. A 1px ring
  // would be antialiased to look semi-transparent, which is not what we want.

  // Resize bounds and border radius around inner ring
  gfx::RectF white_ring_bounds(bounds);
  white_ring_bounds.Inset(-(stroke_width / 2), -(stroke_width / 2));
  int white_ring_border_radius = border_radius + (stroke_width / 2);

  flags.setColor(SK_ColorWHITE);
  flags.setStrokeWidth(stroke_width);
  recorder.canvas()->DrawRoundRect(white_ring_bounds, white_ring_border_radius,
                                   flags);

  // Draw the innermost solid ring
  flags.setColor(highlight_color);
  recorder.canvas()->DrawRoundRect(bounds, border_radius, flags);
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
  if (time_since_focus_move > persist_time_ + fade_out_time_) {
    RemoveLayer();
    return;
  }

  // Compute the opacity based on the fade in and fade out times.
  // TODO(aboxhall): figure out how to use cubic beziers
  float opacity = 1.0f;
  if (time_since_layer_create < fade_in_time_) {
    // We're fading in.
    opacity = time_since_layer_create.InSecondsF() / fade_in_time_.InSecondsF();
  } else if (time_since_focus_move > persist_time_) {
    // Fading out.
    float time_since_began_fading =
        time_since_focus_move.InSecondsF() -
        (fade_in_time_.InSecondsF() + persist_time_.InSecondsF());
    float fade_out_time_float = fade_out_time_.InSecondsF();

    opacity = 1.0f - (time_since_began_fading / fade_out_time_float);
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
