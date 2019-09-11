// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_view.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Appearance.
constexpr int kAssistantIconSizeDip = 16;
constexpr int kCloseButtonIconSizeDip = 16;
constexpr int kCloseButtonSizeDip = 32;
constexpr int kLineHeightDip = 20;
constexpr int kMaxWidthDip = 240;
constexpr int kPaddingLeftDip = 8;
constexpr int kPaddingRightDip = 0;
constexpr int kPreferredHeightDip = 32;

}  // namespace

ProactiveSuggestionsView::ProactiveSuggestionsView(
    AssistantViewDelegate* delegate)
    : views::Button(/*listener=*/this), delegate_(delegate) {
  InitLayout();
  InitWidget();
  InitWindow();

  delegate_->AddUiModelObserver(this);
}

ProactiveSuggestionsView::~ProactiveSuggestionsView() {
  delegate_->RemoveUiModelObserver(this);

  if (GetWidget() && GetWidget()->GetNativeWindow())
    GetWidget()->GetNativeWindow()->RemoveObserver(this);
}

const char* ProactiveSuggestionsView::GetClassName() const {
  return "ProactiveSuggestionsView";
}

gfx::Size ProactiveSuggestionsView::CalculatePreferredSize() const {
  int preferred_width = views::View::CalculatePreferredSize().width();
  preferred_width = std::min(preferred_width, kMaxWidthDip);
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int ProactiveSuggestionsView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void ProactiveSuggestionsView::OnPaintBackground(gfx::Canvas* canvas) {
  const int radius = height() / 2;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(gfx::kGoogleGrey800);
  flags.setDither(true);

  canvas->DrawRoundRect(GetLocalBounds(), radius, flags);
}

void ProactiveSuggestionsView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  // There are two possible |senders|, the close button...
  if (sender == close_button_) {
    delegate_->OnProactiveSuggestionsCloseButtonPressed();
    return;
  }

  // ...and the proactive suggestions view itself.
  DCHECK_EQ(this, sender);
  delegate_->OnProactiveSuggestionsViewPressed();
}

void ProactiveSuggestionsView::OnUsableWorkAreaChanged(
    const gfx::Rect& usable_work_area) {
  UpdateBounds();
}

void ProactiveSuggestionsView::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

void ProactiveSuggestionsView::OnWindowVisibilityChanging(aura::Window* window,
                                                          bool visible) {
  if (!visible) {
    // When exiting, the proactive suggestions window fades out.
    wm::SetWindowVisibilityAnimationType(
        window, wm::WindowVisibilityAnimationType::
                    WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    return;
  }

  // When entering, the proactive suggestions window translates in vertically.
  wm::SetWindowVisibilityAnimationType(
      window, wm::WindowVisibilityAnimationType::
                  WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);

  // The vertical position is equal to the distance from the top of the
  // proactive suggestions view to the bottom of the screen. This gives the
  // effect of animating the proactive suggestions window up from offscreen.
  wm::SetWindowVisibilityAnimationVerticalPosition(
      window, display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(window)
                      .bounds()
                      .bottom() -
                  GetBoundsInScreen().y());
}

void ProactiveSuggestionsView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingLeftDip, 0, kPaddingRightDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Assistant icon.
  views::ImageView* assistant_icon = new views::ImageView();
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      ash::kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
  assistant_icon->SetPreferredSize(
      gfx::Size(kAssistantIconSizeDip, kAssistantIconSizeDip));
  AddChildView(assistant_icon);

  // Spacing.
  // Note that we don't add similar spacing between |label_| and the
  // |close_button_| as the latter has internal spacing between its icon and
  // outer bounds so as to provide a larger hit rect to the user.
  views::View* spacing = new views::View();
  spacing->SetPreferredSize(gfx::Size(kSpacingDip, kPreferredHeightDip));
  AddChildView(spacing);

  // Label.
  views::Label* label = new views::Label();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  label->SetEnabledColor(gfx::kGoogleGrey100);
  label->SetFontList(
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetLineHeight(kLineHeightDip);
  label->SetMultiLine(false);

  // The |description| string coming from the proactive suggestions server may
  // be HTML escaped so we need to unescape before displaying to avoid printing
  // HTML entities to the user.
  label->SetText(
      net::UnescapeForHTML(base::UTF8ToUTF16(delegate_->GetSuggestionsModel()
                                                 ->GetProactiveSuggestions()
                                                 ->description())));

  AddChildView(label);

  // We impose a maximum width restriction on the proactive suggestions view.
  // When restricting width, |label| should cede layout space to its siblings.
  layout_manager->SetFlexForView(label, 1);

  // Close button.
  close_button_ = new views::ImageButton(/*listener=*/this);
  close_button_->SetImage(
      views::ImageButton::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseButtonIconSizeDip,
                            gfx::kGoogleGrey100));
  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  close_button_->SetPreferredSize(
      gfx::Size(kCloseButtonSizeDip, kCloseButtonSizeDip));
  AddChildView(close_button_);
}

void ProactiveSuggestionsView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.context = delegate_->GetRootWindowForNewWindows();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);

  UpdateBounds();
}

void ProactiveSuggestionsView::InitWindow() {
  auto* window = GetWidget()->GetNativeWindow();

  // Initialize the transition duration of the entry/exit animations.
  constexpr int kAnimationDurationMs = 350;
  wm::SetWindowVisibilityAnimationDuration(
      window, base::TimeDelta::FromMilliseconds(kAnimationDurationMs));

  // There is no window property support for modifying entry/exit animation
  // tween type so we set our desired value directly on the LayerAnimator.
  window->layer()->GetAnimator()->set_tween_type(
      gfx::Tween::Type::FAST_OUT_SLOW_IN);

  // We observe the window in order to modify animation behavior prior to window
  // visibility changes. This needs to be done dynamically as bounds are not
  // fully initialized yet for calculating offset position and the animation
  // behavior for exit should only be set once the enter animation is completed.
  window->AddObserver(this);
}

void ProactiveSuggestionsView::UpdateBounds() {
  const gfx::Rect& usable_work_area =
      delegate_->GetUiModel()->usable_work_area();

  const gfx::Size size = CalculatePreferredSize();

  const int left = usable_work_area.x();
  const int top = usable_work_area.bottom() - size.height();

  GetWidget()->SetBounds(gfx::Rect(left, top, size.width(), size.height()));
}

}  // namespace ash
