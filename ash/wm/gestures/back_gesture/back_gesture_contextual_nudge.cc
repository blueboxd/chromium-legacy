// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
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

// Duration of the pause before sliding in to show the nudge.
constexpr base::TimeDelta kPauseBeforeShowAnimationDuration =
    base::TimeDelta::FromSeconds(2);

// Duration for the animation to show the nudge.
constexpr base::TimeDelta kNudgeShowAnimationDuration =
    base::TimeDelta::FromMilliseconds(600);

// Duration for the animation to hide the nudge.
constexpr base::TimeDelta kNudgeHideAnimationDuration =
    base::TimeDelta::FromMilliseconds(400);

// Duration for the animation of the suggestion part of the nudge.
constexpr base::TimeDelta kSuggestionBounceAnimationDuration =
    base::TimeDelta::FromMilliseconds(600);

// Repeat bouncing times of the suggestion animation.
constexpr int kSuggestionAnimationRepeatTimes = 4;

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
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  gfx::Rect widget_bounds(-kBackgroundWidth, display_bounds.y(),
                          kBackgroundWidth, display_bounds.height());
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

class ContextualNudgeView : public views::View,
                            public ui::ImplicitAnimationObserver {
 public:
  ContextualNudgeView() : suggestion_view_(new SuggestionView(this)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    gradient_layer_delegate_ = std::make_unique<GradientLayerDelegate>();
    layer()->SetMaskLayer(gradient_layer_delegate_->layer());

    AddChildView(suggestion_view_);

    show_timer_.Start(
        FROM_HERE, kPauseBeforeShowAnimationDuration, this,
        &ContextualNudgeView::ScheduleOffScreenToStartPositionAnimation);
  }

  ~ContextualNudgeView() override { StopObservingImplicitAnimations(); }

 private:
  // Used to show the suggestion information of the nudge, which includes the
  // affordance and a label.
  class SuggestionView : public views::View,
                         public ui::ImplicitAnimationObserver {
   public:
    explicit SuggestionView(ContextualNudgeView* nudge_view)
        : label_(new views::Label), nudge_view_(nudge_view) {
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);

      label_->SetBackgroundColor(SK_ColorTRANSPARENT);
      label_->SetEnabledColor(kLabelColor);
      label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_BACK_GESTURE_CONTEXTUAL_NUDGE));
      label_->SetMultiLine(true);
      label_->SetFontList(
          gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
      AddChildView(label_);
    }
    ~SuggestionView() override { StopObservingImplicitAnimations(); }

    void ScheduleBounceAnimation() {
      gfx::Transform transform;
      const int x_offset = kCircleRadius - kCircleInsideScreenWidth;
      transform.Translate(-x_offset, 0);
      ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
      animation.AddObserver(this);
      animation.SetTransitionDuration(kSuggestionBounceAnimationDuration);
      animation.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
      animation.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      layer()->SetTransform(transform);

      transform.Translate(x_offset, 0);
      animation.SetTransitionDuration(kSuggestionBounceAnimationDuration);
      animation.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
      animation.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      layer()->SetTransform(transform);
    }

   private:
    // views::View:
    void Layout() override {
      const gfx::Rect bounds = GetLocalBounds();
      gfx::Rect label_rect(bounds);
      label_rect.ClampToCenteredSize(gfx::Size(kLabelWidth, kLabelHeight));
      label_rect.set_x(bounds.left_center().x() + 2 * kCircleRadius +
                       kPaddingBetweenCircleAndLabel);
      label_->SetBoundsRect(label_rect);
    }

    // views::View:
    void OnPaint(gfx::Canvas* canvas) override {
      // Draw the circle.
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(kCircleColor);
      const gfx::Point left_center = GetLocalBounds().left_center();
      canvas->DrawCircle(
          gfx::Point(left_center.x() + kCircleRadius, left_center.y()),
          kCircleRadius, flags);
    }

    // ui::ImplicitAnimationObserver:
    void OnImplicitAnimationsCompleted() override {
      if (current_animation_times_ < (kSuggestionAnimationRepeatTimes - 1)) {
        ScheduleBounceAnimation();
        current_animation_times_++;
      } else {
        nudge_view_->ScheduleStartPositionToOffScreenAnimation();
      }
    }

    views::Label* label_ = nullptr;
    int current_animation_times_ = 0;
    ContextualNudgeView* nudge_view_ = nullptr;  // Not owned.

    SuggestionView(const SuggestionView&) = delete;
    SuggestionView& operator=(const SuggestionView&) = delete;
  };

  // Showing contextual nudge from off screen to its start position.
  void ScheduleOffScreenToStartPositionAnimation() {
    gfx::Transform transform;
    transform.Translate(kBackgroundWidth, 0);
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.AddObserver(this);
    animation.SetTransitionDuration(kNudgeShowAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer()->SetTransform(transform);
  }

  // Hiding the contextual nudge from its current position to off screen.
  void ScheduleStartPositionToOffScreenAnimation() {
    gfx::Transform transform;
    transform.Translate(-kBackgroundWidth, 0);
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(kNudgeHideAnimationDuration);
    animation.SetTweenType(gfx::Tween::EASE_OUT_2);
    layer()->SetTransform(transform);
  }

  // views::View:
  void Layout() override {
    const gfx::Rect bounds = GetLocalBounds();
    gradient_layer_delegate_->layer()->SetBounds(bounds);

    gfx::Rect rect(bounds);
    rect.ClampToCenteredSize(gfx::Size(kBackgroundWidth, kBackgroundWidth));
    rect.set_x(-kCircleRadius);
    suggestion_view_->SetBoundsRect(rect);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    canvas->DrawColor(kBackgroundColor);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    suggestion_view_->ScheduleBounceAnimation();
  }

  std::unique_ptr<GradientLayerDelegate> gradient_layer_delegate_;

  // Created by ContextualNudgeView. Owned by views hierarchy.
  SuggestionView* suggestion_view_ = nullptr;

  // Timer to start show the sliding in animation.
  base::OneShotTimer show_timer_;

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
