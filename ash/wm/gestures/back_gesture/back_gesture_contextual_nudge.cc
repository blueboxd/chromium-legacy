// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Width of the contextual nudge.
constexpr int kBackgroundWidth = 160;

// Color of the contextual nudge.
constexpr SkColor kBackgroundColor = SkColorSetA(SK_ColorBLACK, 153);  // 60%

// Radius of the circle in the middle of the contextual nudge.
constexpr int kCircleRadius = 20;

// Color of the circle in the middle of the contextual nudge.
constexpr SkColor kCircleColor = SK_ColorWHITE;

// Width of the circle that inside the screen at the beginning.
constexpr int kCircleInsideScreenWidth = 12;

// Padding between the circle and the label.
constexpr int kPaddingBetweenCircleAndLabel = 8;

// Color of the label.
constexpr SkColor kLabelColor = gfx::kGoogleGrey200;

// Width and height of the label.
constexpr int kLabelWidth = 80;
constexpr int kLabelHeight = 80;

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.accept_events = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "BackGestureContextualNudge";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  widget->Init(std::move(params));

  // TODO(crbug.com/1009005): Get the bounds of the display that should show the
  // nudge, which may based on the conditions to show the nudge.
  gfx::Rect widget_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  widget_bounds.set_width(kBackgroundWidth);
  widget->SetBounds(widget_bounds);
  return widget;
}

class GradientLayerDelegate : public ui::LayerDelegate {
 public:
  GradientLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~GradientLayerDelegate() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size size = layer()->size();
    ui::PaintRecorder recorder(context, size);

    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);
    flags.setShader(
        gfx::CreateGradientShader(gfx::Point(), gfx::Point(size.width(), 0),
                                  SK_ColorBLACK, SK_ColorTRANSPARENT));
    recorder.canvas()->DrawRect(gfx::Rect(gfx::Point(), size), flags);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;

  GradientLayerDelegate(const GradientLayerDelegate&) = delete;
  GradientLayerDelegate& operator=(const GradientLayerDelegate&) = delete;
};

class ContextualNudgeView : public views::View {
 public:
  ContextualNudgeView() : label_(new views::Label) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    gradient_layer_delegate_ = std::make_unique<GradientLayerDelegate>();
    layer()->SetMaskLayer(gradient_layer_delegate_->layer());

    label_->SetBackgroundColor(SK_ColorTRANSPARENT);
    label_->SetEnabledColor(kLabelColor);
    label_->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_BACK_GESTURE_CONTEXTUAL_NUDGE));
    label_->SetMultiLine(true);
    label_->SetFontList(
        gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    AddChildView(label_);
  }

  ~ContextualNudgeView() override = default;

 private:
  // views::View:
  void Layout() override {
    const gfx::Rect bounds = GetLocalBounds();
    gradient_layer_delegate_->layer()->SetBounds(bounds);

    gfx::Rect label_rect(bounds);
    label_rect.ClampToCenteredSize(gfx::Size(kLabelWidth, kLabelHeight));
    label_rect.set_x(bounds.left_center().x() + kCircleInsideScreenWidth +
                     kPaddingBetweenCircleAndLabel);
    label_->SetBoundsRect(label_rect);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    canvas->DrawColor(kBackgroundColor);

    // Draw the circle.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kCircleColor);
    gfx::Point left_center = layer()->bounds().left_center();
    canvas->DrawCircle(
        gfx::Point(kCircleInsideScreenWidth - kCircleRadius, left_center.y()),
        kCircleRadius, flags);
  }

  std::unique_ptr<GradientLayerDelegate> gradient_layer_delegate_;

  views::Label* label_ = nullptr;

  ContextualNudgeView(const ContextualNudgeView&) = delete;
  ContextualNudgeView& operator=(const ContextualNudgeView&) = delete;
};

}  // namespace

BackGestureContextualNudge::BackGestureContextualNudge() {
  if (!widget_)
    widget_ = CreateWidget();

  widget_->SetContentsView(new ContextualNudgeView());
  widget_->Show();
}

BackGestureContextualNudge::~BackGestureContextualNudge() = default;

}  // namespace ash
