// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnifier_glass.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Size of the border around the magnifying glass in DIP.
constexpr int kBorderSize = 10;
// Thickness of the outline around magnifying glass border in DIP.
constexpr int kBorderOutlineThickness = 1;
// Thickness of the shadow around the magnifying glass in DIP.
constexpr int kShadowThickness = 24;
// Offset of the shadow around the magnifying glass in DIP. One of the shadows
// is lowered a bit, so we have to include |kShadowOffset| in our calculations
// to compensate.
constexpr int kShadowOffset = 24;
// The color of the border and its outlines. The border has an outline on both
// sides, producing a black/white/black ring.
constexpr SkColor kBorderColor = SkColorSetARGB(204, 255, 255, 255);
constexpr SkColor kBorderOutlineColor = SkColorSetARGB(51, 0, 0, 0);
// The colors of the two shadow around the magnifiying glass.
constexpr SkColor kTopShadowColor = SkColorSetARGB(26, 0, 0, 0);
constexpr SkColor kBottomShadowColor = SkColorSetARGB(61, 0, 0, 0);
// Inset on the zoom filter.
constexpr int kZoomInset = 0;
// Vertical offset between the center of the magnifier and the tip of the
// pointer. TODO(jdufault): The vertical offset should only apply to the window
// location, not the magnified contents. See crbug.com/637617.
constexpr int kVerticalOffset = 0;

// Name of the magnifier window.
constexpr char kMagniferGlassWindowName[] = "MagnifierGlassWindow";

gfx::Size GetWindowSize(const MagnifierGlass::Params& params) {
  // The diameter of the window is the diameter of the magnifier, border and
  // shadow combined. We apply |kShadowOffset| on all sides even though the
  // shadow is only thicker on the bottom so as to keep the circle centered in
  // the view and keep calculations (border rendering and content masking)
  // simpler.
  int window_diameter =
      (params.radius + kBorderSize + kShadowThickness + kShadowOffset) * 2;
  return gfx::Size(window_diameter, window_diameter);
}

gfx::Rect GetBounds(const MagnifierGlass::Params& params,
                    const gfx::Point& point) {
  gfx::Size size = GetWindowSize(params);
  gfx::Point origin(point.x() - (size.width() / 2),
                    point.y() - (size.height() / 2) - kVerticalOffset);
  return gfx::Rect(origin, size);
}

}  // namespace

// The border mask provides a clipping layer for the magnification window
// border so that there is no shadow in the center.
class MagnifierGlass::BorderMask : public ui::LayerDelegate {
 public:
  BorderMask(const gfx::Size& mask_bounds, int radius)
      : radius_(radius), layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
    layer_.SetBounds(gfx::Rect(mask_bounds));
  }

  ~BorderMask() override { layer_.set_delegate(nullptr); }

  BorderMask(const BorderMask& other) = delete;
  BorderMask& operator=(const BorderMask& rhs) = delete;

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer()->size());

    cc::PaintFlags flags;
    flags.setAlpha(255);
    flags.setAntiAlias(true);
    // Stroke is used for clipping the border which consists of the rendered
    // border |kBorderSize| and the magnifier shadow |kShadowThickness| and
    // |kShadowOffset|.
    constexpr int kStrokeWidth = kBorderSize + kShadowThickness + kShadowOffset;
    flags.setStrokeWidth(kStrokeWidth);
    flags.setStyle(cc::PaintFlags::kStroke_Style);

    // We want to clip everything but the border, shadow and shadow offset, so
    // set the clipping radius at the halfway point of the stroke width.
    const int kClippingRadius = radius_ + (kStrokeWidth) / 2;
    const gfx::Rect rect(layer()->bounds().size());
    recorder.canvas()->DrawCircle(rect.CenterPoint(), kClippingRadius, flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    // Redrawing will take care of scale factor change.
  }

  const int radius_;
  ui::Layer layer_;
};

// The border renderer draws the border as well as outline on both the outer and
// inner radius to increase visibility. The border renderer also handles drawing
// the shadow.
class MagnifierGlass::BorderRenderer : public ui::LayerDelegate {
 public:
  BorderRenderer(const gfx::Rect& window_bounds, int radius)
      : magnifier_window_bounds_(window_bounds), radius_(radius) {
    magnifier_shadows_.push_back(gfx::ShadowValue(
        gfx::Vector2d(0, kShadowOffset), kShadowThickness, kBottomShadowColor));
    magnifier_shadows_.push_back(gfx::ShadowValue(
        gfx::Vector2d(0, 0), kShadowThickness, kTopShadowColor));
  }

  ~BorderRenderer() override = default;

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, magnifier_window_bounds_.size());

    // Draw the shadow.
    cc::PaintFlags shadow_flags;
    shadow_flags.setAntiAlias(true);
    shadow_flags.setColor(SK_ColorTRANSPARENT);
    shadow_flags.setLooper(gfx::CreateShadowDrawLooper(magnifier_shadows_));
    gfx::Rect shadow_bounds(magnifier_window_bounds_.size());
    recorder.canvas()->DrawCircle(
        shadow_bounds.CenterPoint(),
        shadow_bounds.width() / 2 - kShadowThickness - kShadowOffset,
        shadow_flags);

    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);

    // The radius of the magnifier and its border.
    const int magnifier_radius = radius_ + kBorderSize;

    // Draw the inner border.
    border_flags.setStrokeWidth(kBorderSize);
    border_flags.setColor(kBorderColor);
    recorder.canvas()->DrawCircle(magnifier_window_bounds_.CenterPoint(),
                                  magnifier_radius - kBorderSize / 2,
                                  border_flags);

    // Draw border outer outline and then draw the border inner outline.
    border_flags.setStrokeWidth(kBorderOutlineThickness);
    border_flags.setColor(kBorderOutlineColor);
    recorder.canvas()->DrawCircle(
        magnifier_window_bounds_.CenterPoint(),
        magnifier_radius - kBorderOutlineThickness / 2, border_flags);
    recorder.canvas()->DrawCircle(
        magnifier_window_bounds_.CenterPoint(),
        magnifier_radius - kBorderSize + kBorderOutlineThickness / 2,
        border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  const gfx::Rect magnifier_window_bounds_;
  const int radius_;
  std::vector<gfx::ShadowValue> magnifier_shadows_;

  DISALLOW_COPY_AND_ASSIGN(BorderRenderer);
};

MagnifierGlass::MagnifierGlass(Params params) : params_(std::move(params)) {}

MagnifierGlass::~MagnifierGlass() {
  CloseMagnifierWindow();
}

void MagnifierGlass::ShowFor(aura::Window* root_window,
                             const gfx::Point& location_in_root) {
  if (!host_widget_) {
    CreateMagnifierWindow(root_window, location_in_root);
    return;
  }

  if (root_window != host_widget_->GetNativeView()->GetRootWindow()) {
    CloseMagnifierWindow();
    CreateMagnifierWindow(root_window, location_in_root);
    return;
  }

  host_widget_->SetBounds(GetBounds(params_, location_in_root));
}

void MagnifierGlass::Close() {
  CloseMagnifierWindow();
}

void MagnifierGlass::OnWindowDestroying(aura::Window* window) {
  CloseMagnifierWindow();
}

void MagnifierGlass::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, host_widget_);
  RemoveZoomWidgetObservers();
  host_widget_ = nullptr;
}

void MagnifierGlass::CreateMagnifierWindow(aura::Window* root_window,
                                           const gfx::Point& location_in_root) {
  if (host_widget_ || !root_window)
    return;

  root_window->AddObserver(this);

  host_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.bounds = GetBounds(params_, location_in_root);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = kMagniferGlassWindowName;
  params.parent = root_window;
  host_widget_->Init(std::move(params));
  host_widget_->set_focus_on_creation(false);
  host_widget_->Show();

  ui::Layer* root_layer = host_widget_->GetNativeView()->layer();

  const gfx::Size window_size = GetWindowSize(params_);
  const gfx::Rect window_bounds = gfx::Rect(window_size);

  zoom_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  zoom_layer_->SetBounds(window_bounds);
  zoom_layer_->SetBackgroundZoom(params_.scale, kZoomInset);
  zoom_layer_->SetFillsBoundsOpaquely(false);
  root_layer->Add(zoom_layer_.get());

  // Create a rounded rect clip, so that only we see a circle of the zoomed
  // content. This circle radius should match that of the drawn border.
  const gfx::RoundedCornersF kRoundedCorners{params_.radius};
  zoom_layer_->SetRoundedCornerRadius(kRoundedCorners);
  gfx::Rect clip_rect = window_bounds;
  clip_rect.ClampToCenteredSize(
      gfx::Size(params_.radius * 2, params_.radius * 2));
  zoom_layer_->SetClipRect(clip_rect);

  border_layer_ = std::make_unique<ui::Layer>();
  border_layer_->SetBounds(window_bounds);
  border_renderer_ =
      std::make_unique<BorderRenderer>(window_bounds, params_.radius);
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  root_layer->Add(border_layer_.get());

  border_mask_ = std::make_unique<BorderMask>(window_size, params_.radius);
  border_layer_->SetMaskLayer(border_mask_->layer());

  host_widget_->AddObserver(this);
}

void MagnifierGlass::CloseMagnifierWindow() {
  if (host_widget_) {
    RemoveZoomWidgetObservers();
    host_widget_->Close();
    host_widget_ = nullptr;
  }
}

void MagnifierGlass::RemoveZoomWidgetObservers() {
  DCHECK(host_widget_);
  host_widget_->RemoveObserver(this);
  aura::Window* root_window = host_widget_->GetNativeView()->GetRootWindow();
  DCHECK(root_window);
  root_window->RemoveObserver(this);
}

}  // namespace ash
