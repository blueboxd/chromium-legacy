// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_drag_indicators.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_highlight_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// When a preview is shown, the opposite highlight shall contract to this ratio
// of the screen length.
constexpr float kOtherHighlightScreenPrimaryAxisRatio = 0.03f;

// Creates the widget responsible for displaying the indicators.
std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent = Shell::GetContainer(Shell::Get()->GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  return widget;
}

// Computes the transform which rotates the labels |angle| degrees. The point
// of rotation is the relative center point of |bounds|.
gfx::Transform ComputeRotateAroundCenterTransform(const gfx::Rect& bounds,
                                                  double angle) {
  gfx::Transform transform;
  const gfx::Vector2dF center_point_vector =
      bounds.CenterPoint() - bounds.origin();
  transform.Translate(center_point_vector);
  transform.Rotate(angle);
  transform.Translate(-center_point_vector);
  return transform;
}

// Returns the work area bounds that has no overlap with shelf.
gfx::Rect GetWorkAreaBoundsNoOverlapWithShelf(aura::Window* root_window) {
  aura::Window* window =
      root_window->GetChildById(kShellWindowId_OverlayContainer);
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(window);
  ::wm::ConvertRectToScreen(root_window, &bounds);

  bounds.Subtract(Shelf::ForWindow(root_window)->GetIdealBounds());
  return bounds;
}

}  // namespace

// static
bool SplitViewDragIndicators::IsPreviewAreaState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kPreviewAreaLeft ||
         indicator_state == IndicatorState::kPreviewAreaRight;
}

// static
bool SplitViewDragIndicators::IsLeftIndicatorState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kDragAreaLeft ||
         indicator_state == IndicatorState::kCannotSnapLeft;
}

// static
bool SplitViewDragIndicators::IsRightIndicatorState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kDragAreaRight ||
         indicator_state == IndicatorState::kCannotSnapRight;
}

// static
bool SplitViewDragIndicators::IsCannotSnapState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kCannotSnap ||
         indicator_state == IndicatorState::kCannotSnapLeft ||
         indicator_state == IndicatorState::kCannotSnapRight;
}

// static
bool SplitViewDragIndicators::IsPreviewAreaOnLeftTopOfScreen(
    IndicatorState indicator_state) {
  // kPreviewAreaLeft and kPreviewAreaRight correspond with LEFT_SNAPPED and
  // RIGHT_SNAPPED which do not always correspond to the physical left and right
  // of the screen. See split_view_controller.h for more details.
  return (indicator_state == IndicatorState::kPreviewAreaLeft &&
          IsCurrentScreenOrientationPrimary()) ||
         (indicator_state == IndicatorState::kPreviewAreaRight &&
          !IsCurrentScreenOrientationPrimary());
}

// View which contains a label and can be rotated. Used by and rotated by
// SplitViewDragIndicatorsView.
class SplitViewDragIndicators::RotatedImageLabelView : public views::View {
 public:
  explicit RotatedImageLabelView(bool is_right_or_bottom)
      : is_right_or_bottom_(is_right_or_bottom) {
    label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetEnabledColor(kSplitviewLabelEnabledColor);
    label_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

    // Use |label_parent_| to add padding and rounded edges to the text. Create
    // this extra view so that we can rotate the label, while having a slide
    // animation at times on the whole thing.
    label_parent_ = new RoundedRectView(kSplitviewLabelRoundRectRadiusDp,
                                        kSplitviewLabelBackgroundColor);
    label_parent_->SetPaintToLayer();
    label_parent_->layer()->SetFillsBoundsOpaquely(false);
    label_parent_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));
    label_parent_->AddChildView(label_);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    AddChildView(label_parent_);
  }

  ~RotatedImageLabelView() override = default;

  void SetLabelText(const base::string16& text) { label_->SetText(text); }

  // Called when the view's bounds are altered. Rotates the view by |angle|
  // degrees.
  void OnBoundsUpdated(const gfx::Rect& bounds, double angle) {
    SetBoundsRect(bounds);
    label_parent_->SetBoundsRect(gfx::Rect(bounds.size()));
    label_parent_->SetTransform(
        ComputeRotateAroundCenterTransform(bounds, angle));
  }

  // Called to update the opacity of the labels view on |indicator_state|.
  void OnIndicatorTypeChanged(IndicatorState indicator_state,
                              IndicatorState previous_indicator_state) {
    // In split view, the labels never show, and they do not need to be updated.
    if (Shell::Get()->split_view_controller()->InSplitViewMode())
      return;

    // On transition to a state with no indicators, any label that is showing
    // shall fade out with the corresponding indicator. The following call to
    // DoSplitviewOpacityAnimation() will do nothing if this, in the sense of
    // the C++ keyword this, already has zero opacity.
    if (indicator_state == IndicatorState::kNone) {
      DoSplitviewOpacityAnimation(
          layer(), SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT);
      return;
    }

    // On transition to a state with a preview area, any label that is showing
    // shall fade out. The following call to DoSplitviewOpacityAnimation() will
    // do nothing if this, in the sense of the C++ keyword this, already has
    // zero opacity (for example, on transition from kDragAreaLeft to
    // kPreviewAreaLeft, if this is the label on the right).
    if (IsPreviewAreaState(indicator_state)) {
      DoSplitviewOpacityAnimation(layer(), SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
      return;
    }

    // Having ruled out kNone and the "preview area" states, we know that
    // |indicator_state| is either a "drag area" state or a "cannot snap" state.
    // If there is an indicator on only one side, and if this, in the sense of
    // the C++ keyword this, is the label on the opposite side, then bail out.
    if (is_right_or_bottom_ ? IsLeftIndicatorState(indicator_state)
                            : IsRightIndicatorState(indicator_state)) {
      return;
    }

    // Set the text according to whether |indicator_state| is a "drag area"
    // state or a "cannot snap" state.
    SetLabelText(l10n_util::GetStringUTF16(IsCannotSnapState(indicator_state)
                                               ? IDS_ASH_SPLIT_VIEW_CANNOT_SNAP
                                               : IDS_ASH_SPLIT_VIEW_GUIDANCE));

    // On transition from a state with no indicators, fade in with an indicator.
    if (previous_indicator_state == IndicatorState::kNone) {
      DoSplitviewOpacityAnimation(
          layer(), SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT);
      return;
    }

    // On transition from a state with a preview area, fade in.
    if (IsPreviewAreaState(previous_indicator_state)) {
      DoSplitviewOpacityAnimation(layer(), SPLITVIEW_ANIMATION_TEXT_FADE_IN);
      return;
    }
  }

 protected:
  gfx::Size CalculatePreferredSize() const override {
    return label_parent_->GetPreferredSize();
  }

 private:
  // True if the label view is the right/bottom side one, false if it is the
  // left/top one.
  const bool is_right_or_bottom_;

  RoundedRectView* label_parent_ = nullptr;
  views::Label* label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RotatedImageLabelView);
};

// View which contains two highlights on each side indicator where a user should
// drag a selected window in order to initiate splitview. Each highlight has a
// label with instructions to further guide users. The highlights are on the
// left and right of the display in landscape mode, and on the top and bottom of
// the display in landscape mode. The highlights can expand and shrink if a
// window has entered a snap region to display the bounds of the window, if it
// were to get snapped.
class SplitViewDragIndicators::SplitViewDragIndicatorsView
    : public views::View {
 public:
  SplitViewDragIndicatorsView() {
    left_highlight_view_ =
        new SplitViewHighlightView(/*is_right_or_bottom=*/false);
    right_highlight_view_ =
        new SplitViewHighlightView(/*is_right_or_bottom=*/true);

    left_highlight_view_->SetPaintToLayer();
    right_highlight_view_->SetPaintToLayer();
    left_highlight_view_->layer()->SetFillsBoundsOpaquely(false);
    right_highlight_view_->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(left_highlight_view_);
    AddChildView(right_highlight_view_);

    left_rotated_view_ =
        new RotatedImageLabelView(/*is_right_or_bottom=*/false);
    right_rotated_view_ =
        new RotatedImageLabelView(/*is_right_or_bottom=*/true);

    AddChildView(left_rotated_view_);
    AddChildView(right_rotated_view_);

    // Nothing is shown initially.
    left_highlight_view_->layer()->SetOpacity(0.f);
    right_highlight_view_->layer()->SetOpacity(0.f);
    left_rotated_view_->layer()->SetOpacity(0.f);
    right_rotated_view_->layer()->SetOpacity(0.f);
  }

  ~SplitViewDragIndicatorsView() override {}

  // Called by parent widget when the state machine changes.
  void OnIndicatorTypeChanged(IndicatorState indicator_state) {
    DCHECK_NE(indicator_state_, indicator_state);
    previous_indicator_state_ = indicator_state_;
    indicator_state_ = indicator_state;
    Update();
  }

  // Handles setting the opacity and bounds of the highlights and labels.
  void Update() {
    left_rotated_view_->OnIndicatorTypeChanged(indicator_state_,
                                               previous_indicator_state_);
    right_rotated_view_->OnIndicatorTypeChanged(indicator_state_,
                                                previous_indicator_state_);
    left_highlight_view_->OnIndicatorTypeChanged(indicator_state_,
                                                 previous_indicator_state_);
    right_highlight_view_->OnIndicatorTypeChanged(indicator_state_,
                                                  previous_indicator_state_);

    if (indicator_state_ != IndicatorState::kNone ||
        IsPreviewAreaState(previous_indicator_state_))
      Layout(previous_indicator_state_ != IndicatorState::kNone);
  }

  views::View* GetViewForIndicatorType(IndicatorType type) {
    switch (type) {
      case IndicatorType::kLeftHighlight:
        return left_highlight_view_;
      case IndicatorType::kLeftText:
        return left_rotated_view_;
      case IndicatorType::kRightHighlight:
        return right_highlight_view_;
      case IndicatorType::kRightText:
        return right_rotated_view_;
    }

    NOTREACHED();
    return nullptr;
  }

  // views::View:
  void Layout() override { Layout(/*animate=*/false); }

 private:
  // Layout the bounds of the highlight views and helper labels. One should
  // animate when changing states, but not when bounds or orientation is
  // changed.
  void Layout(bool animate) {
    const bool landscape = IsCurrentScreenOrientationLandscape();
    const int display_width = landscape ? width() : height();
    const int display_height = landscape ? height() : width();

    // Calculate the bounds of the two highlight regions.
    const int highlight_width =
        display_width * kHighlightScreenPrimaryAxisRatio;
    const int highlight_height =
        display_height - 2 * kHighlightScreenEdgePaddingDp;
    gfx::Size highlight_size(highlight_width, highlight_height);

    // When one highlight expands to become a preview area, the other highlight
    // contracts to this width.
    const int other_highlight_width =
        display_width * kOtherHighlightScreenPrimaryAxisRatio;

    // The origin of the right highlight view in landscape, or the bottom
    // highlight view in portrait.
    gfx::Point right_bottom_origin(
        display_width - highlight_width - kHighlightScreenEdgePaddingDp,
        kHighlightScreenEdgePaddingDp);

    const gfx::Point highlight_padding_point(kHighlightScreenEdgePaddingDp,
                                             kHighlightScreenEdgePaddingDp);
    gfx::Rect left_highlight_bounds(highlight_padding_point, highlight_size);
    gfx::Rect right_highlight_bounds(right_bottom_origin, highlight_size);
    if (!landscape) {
      left_highlight_bounds.Transpose();
      right_highlight_bounds.Transpose();
    }
    // While the preview area fades out, its inset animates to zero.
    const bool nix_preview_inset =
        indicator_state_ == IndicatorState::kNone &&
        IsPreviewAreaState(previous_indicator_state_);
    // On transition to or from a preview area state, |preview_state| is that
    // state. |preview_state| and anything directly or indirectly computed
    // therefrom is intended for purposes related to such transitions.
    const IndicatorState preview_state = IsPreviewAreaState(indicator_state_)
                                             ? indicator_state_
                                             : previous_indicator_state_;
    // |preview_left_or_top| indicates a preview area on the physical left or
    // top of the screen (corresponding to kPreviewAreaLeft in primary screen
    // orientations, and kPreviewAreaRight in nonprimary screen orientations).
    const bool preview_left_or_top =
        IsPreviewAreaOnLeftTopOfScreen(preview_state);
    gfx::Rect preview_area_bounds;
    base::Optional<SplitviewAnimationType> left_highlight_animation_type;
    base::Optional<SplitviewAnimationType> right_highlight_animation_type;
    if (IsPreviewAreaState(indicator_state_) || nix_preview_inset) {
      // Get the preview area bounds from the split view controller.
      preview_area_bounds =
          Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInScreen(
              GetWidget()->GetNativeWindow(),
              preview_state == IndicatorState::kPreviewAreaLeft
                  ? SplitViewController::LEFT
                  : SplitViewController::RIGHT);

      aura::Window* root_window =
          GetWidget()->GetNativeWindow()->GetRootWindow();
      // Preview area should have no overlap with the shelf.
      preview_area_bounds.Subtract(
          Shelf::ForWindow(root_window)->GetIdealBounds());

      const gfx::Rect work_area_bounds =
          GetWorkAreaBoundsNoOverlapWithShelf(root_window);
      preview_area_bounds.set_y(preview_area_bounds.y() - work_area_bounds.y());
      if (!nix_preview_inset) {
        preview_area_bounds.Inset(kHighlightScreenEdgePaddingDp,
                                  kHighlightScreenEdgePaddingDp);
      }

      // Calculate the bounds of the other highlight, which is the one that
      // shrinks and fades away, while the other one, the preview area, expands
      // and takes up half the screen.
      gfx::Rect other_bounds(
          display_width - other_highlight_width - kHighlightScreenEdgePaddingDp,
          kHighlightScreenEdgePaddingDp, other_highlight_width,
          display_height - 2 * kHighlightScreenEdgePaddingDp);
      if (!landscape)
        other_bounds.Transpose();

      if (preview_left_or_top) {
        left_highlight_bounds = preview_area_bounds;
        right_highlight_bounds = other_bounds;
        if (animate) {
          if (nix_preview_inset) {
            left_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET;
          } else {
            left_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN;
            right_highlight_animation_type =
                SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT;
          }
        }
      } else {
        other_bounds.set_origin(highlight_padding_point);
        left_highlight_bounds = other_bounds;
        right_highlight_bounds = preview_area_bounds;
        if (animate) {
          if (nix_preview_inset) {
            right_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET;
          } else {
            left_highlight_animation_type =
                SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT;
            right_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN;
          }
        }
      }
    } else if (IsPreviewAreaState(previous_indicator_state_) && animate) {
      if (preview_left_or_top) {
        left_highlight_animation_type =
            SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT;
        right_highlight_animation_type =
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN;
      } else {
        left_highlight_animation_type =
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN;
        right_highlight_animation_type =
            SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT;
      }
    }

    left_highlight_view_->SetBounds(GetMirroredRect(left_highlight_bounds),
                                    landscape, left_highlight_animation_type);
    right_highlight_view_->SetBounds(GetMirroredRect(right_highlight_bounds),
                                     landscape, right_highlight_animation_type);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in landscape mode.
    const int size_width =
        indicator_state_ == IndicatorState::kDragAreaLeft
            ? left_rotated_view_->GetPreferredSize().width()
            : right_rotated_view_->GetPreferredSize().width();
    gfx::Size size(size_width, kSplitviewLabelPreferredHeightDp);
    if (!landscape)
      highlight_size.SetSize(highlight_size.height(), highlight_size.width());
    gfx::Rect left_rotated_bounds(
        highlight_size.width() / 2 - size.width() / 2,
        highlight_size.height() / 2 - size.height() / 2, size.width(),
        size.height());
    gfx::Rect right_rotated_bounds = left_rotated_bounds;
    left_rotated_bounds.Offset(highlight_padding_point.x(),
                               highlight_padding_point.y());
    if (!landscape) {
      right_bottom_origin.SetPoint(right_bottom_origin.y(),
                                   right_bottom_origin.x());
    }
    right_rotated_bounds.Offset(right_bottom_origin.x(),
                                right_bottom_origin.y());

    // In portrait mode, there is no need to rotate the text and warning icon.
    // In landscape mode, rotate the left text 90 degrees clockwise in rtl and
    // 90 degress anti clockwise in ltr. The right text is rotated 90 degrees in
    // the opposite direction of the left text.
    double left_rotation_angle = 0.0;
    if (landscape)
      left_rotation_angle = 90.0 * (base::i18n::IsRTL() ? 1 : -1);

    left_rotated_view_->OnBoundsUpdated(left_rotated_bounds,
                                        /*angle=*/left_rotation_angle);
    right_rotated_view_->OnBoundsUpdated(right_rotated_bounds,
                                         /*angle=*/-left_rotation_angle);

    // On transition from a preview area state to kNone (in other words, on
    // snapping a window), reset the label transforms in preparation for a later
    // transition from kNone.
    if (nix_preview_inset) {
      left_rotated_view_->layer()->SetTransform(gfx::Transform());
      right_rotated_view_->layer()->SetTransform(gfx::Transform());
      return;
    }

    ui::Layer* preview_label_layer;
    ui::Layer* other_highlight_label_layer;
    if (preview_left_or_top) {
      preview_label_layer = left_rotated_view_->layer();
      other_highlight_label_layer = right_rotated_view_->layer();
    } else {
      preview_label_layer = right_rotated_view_->layer();
      other_highlight_label_layer = left_rotated_view_->layer();
    }

    // Slide out the labels on transition to a preview area state. (This code
    // also adjusts the label transforms for things like screen rotation while
    // |indicator_state_| is a preview area state.)
    if (IsPreviewAreaState(indicator_state_)) {
      // How far each label shall slide to stay centered in the corresponding
      // highlight as it expands/contracts. Include distance traveled with zero
      // opacity (whence a label still slides, not only for simplicity in
      // calculating the values below, but also to facilitate that the label
      // transform and the highlight transform have matching easing).
      const float preview_label_distance =
          0.5f * (preview_area_bounds.width() - highlight_width);
      const float other_highlight_label_distance =
          0.5f * (highlight_width - other_highlight_width);

      // Positive for right or down; negative for left or up.
      float preview_label_delta, other_highlight_label_delta;
      if (preview_left_or_top) {
        preview_label_delta = preview_label_distance;
        other_highlight_label_delta = other_highlight_label_distance;
      } else {
        preview_label_delta = -preview_label_distance;
        other_highlight_label_delta = -other_highlight_label_distance;
      }

      // x-axis if |landscape|; else y-axis.
      gfx::Transform preview_label_transform, other_highlight_label_transform;
      if (landscape) {
        preview_label_transform.Translate(preview_label_delta, 0.f);
        other_highlight_label_transform.Translate(other_highlight_label_delta,
                                                  0.f);
      } else {
        preview_label_transform.Translate(0.f, preview_label_delta);
        other_highlight_label_transform.Translate(0.f,
                                                  other_highlight_label_delta);
      }

      if (animate) {
        // Animate the labels sliding out.
        DoSplitviewTransformAnimation(
            preview_label_layer,
            SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT,
            preview_label_transform);
        DoSplitviewTransformAnimation(
            other_highlight_label_layer,
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT,
            other_highlight_label_transform);
      } else {
        // Put the labels where they belong.
        preview_label_layer->SetTransform(preview_label_transform);
        other_highlight_label_layer->SetTransform(
            other_highlight_label_transform);
      }
      return;
    }

    // Slide in the labels on transition from a preview area state.
    if (IsPreviewAreaState(previous_indicator_state_)) {
      if (animate) {
        // Animate the labels sliding in.
        DoSplitviewTransformAnimation(
            preview_label_layer, SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN,
            gfx::Transform());
        DoSplitviewTransformAnimation(
            other_highlight_label_layer,
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN,
            gfx::Transform());
      } else {
        // Put the labels where they belong.
        preview_label_layer->SetTransform(gfx::Transform());
        other_highlight_label_layer->SetTransform(gfx::Transform());
      }
      return;
    }
  }

  SplitViewHighlightView* left_highlight_view_ = nullptr;
  SplitViewHighlightView* right_highlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  IndicatorState indicator_state_ = IndicatorState::kNone;
  IndicatorState previous_indicator_state_ = IndicatorState::kNone;

  DISALLOW_COPY_AND_ASSIGN(SplitViewDragIndicatorsView);
};

SplitViewDragIndicators::SplitViewDragIndicators() {
  indicators_view_ = new SplitViewDragIndicatorsView();
  widget_ = CreateWidget();
  widget_->SetContentsView(indicators_view_);
  widget_->Show();
}

SplitViewDragIndicators::~SplitViewDragIndicators() {
  // Allow some extra time for animations to finish.
  aura::Window* window = widget_->GetNativeWindow();
  if (window == nullptr)
    return;
  wm::SetWindowVisibilityAnimationType(
      window, WINDOW_VISIBILITY_ANIMATION_TYPE_STEP_END);
  AnimateOnChildWindowVisibilityChanged(window, /*visible=*/false);
}

void SplitViewDragIndicators::SetIndicatorState(
    IndicatorState indicator_state,
    const gfx::Point& event_location) {
  if (indicator_state == current_indicator_state_)
    return;

  // Reparent the widget if needed.
  aura::Window* target = ash::window_util::GetRootWindowAt(event_location);
  aura::Window* root_window = target->GetRootWindow();
  if (widget_->GetNativeView()->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        widget_->GetNativeView(),
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
    widget_->SetContentsView(indicators_view_);
  }
  widget_->SetBounds(GetWorkAreaBoundsNoOverlapWithShelf(root_window));

  current_indicator_state_ = indicator_state;
  indicators_view_->OnIndicatorTypeChanged(current_indicator_state_);
}

void SplitViewDragIndicators::OnSplitViewModeEnded() {
  if (current_indicator_state_ == IndicatorState::kNone ||
      IsPreviewAreaState(current_indicator_state_)) {
    return;
  }
  indicators_view_->Update();
}

void SplitViewDragIndicators::OnDisplayBoundsChanged() {
  aura::Window* root_window = widget_->GetNativeView()->GetRootWindow();
  widget_->SetBounds(GetWorkAreaBoundsNoOverlapWithShelf(root_window));
}

bool SplitViewDragIndicators::GetIndicatorTypeVisibilityForTesting(
    IndicatorType type) const {
  return indicators_view_->GetViewForIndicatorType(type)->layer()->opacity() >
         0.f;
}

}  // namespace ash
