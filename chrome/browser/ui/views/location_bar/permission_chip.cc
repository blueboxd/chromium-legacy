// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_chip.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "components/permissions/permission_request.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {
bool IsCameraPermission(permissions::PermissionRequestType type) {
  return type ==
         permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA;
}

bool IsCameraOrMicPermission(permissions::PermissionRequestType type) {
  return IsCameraPermission(type) ||
         type == permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_MIC;
}
}  // namespace

PermissionChip::PermissionChip(Browser* browser)
    : views::AnimationDelegateViews(nullptr), browser_(browser) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetVisible(false);

  chip_button_ = AddChildView(views::MdTextButton::Create(
      this, base::string16(), views::style::CONTEXT_BUTTON_MD));
  chip_button_->SetProminent(true);
  chip_button_->SetCornerRadius(GetIconSize());
  chip_button_->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);

  constexpr auto kAnimationDuration = base::TimeDelta::FromMilliseconds(350);
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(kAnimationDuration);
}

PermissionChip::~PermissionChip() {
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
}

void PermissionChip::Show(permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();

  // TODO(olesiamarukhno): Add combined camera & microphone permission and
  // update delegate to contain only one request at a time.
  DCHECK(requests.size() == 1u || requests.size() == 2u);
  if (requests.size() == 2) {
    DCHECK(IsCameraOrMicPermission(requests[0]->GetPermissionRequestType()));
    DCHECK(IsCameraOrMicPermission(requests[1]->GetPermissionRequestType()));
    DCHECK_NE(requests[0]->GetPermissionRequestType(),
              requests[1]->GetPermissionRequestType());
  }

  chip_button_->SetText(GetPermissionMessage());
  UpdatePermissionIconAndTextColor();

  SetVisible(true);
  animation_->Show();
  requested_time_ = base::TimeTicks::Now();
}

void PermissionChip::Hide() {
  SetVisible(false);
  animation_->Hide();
  delegate_ = nullptr;
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
  already_recorded_interaction_ = false;
}

gfx::Size PermissionChip::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + chip_button_->GetInsets().width();
  const int collapsable_width =
      chip_button_->GetPreferredSize().width() - fixed_width;
  const int width =
      std::round(collapsable_width * animation_->GetCurrentValue()) +
      fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void PermissionChip::OnMouseEntered(const ui::MouseEvent& event) {
  // Restart the timer after user hovers the view.
  StartCollapseTimer();
}

void PermissionChip::OnThemeChanged() {
  View::OnThemeChanged();
  UpdatePermissionIconAndTextColor();
}

void PermissionChip::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  DCHECK_EQ(chip_button_, sender);

  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!prompt_bubble_);

  // TODO(olesiamarukhno): Remove ink drop animation when the bubble is opened.
  if (is_bubble_showing_) {
    // If the user clicks on the chip when the bubble is open, they probably
    // don't want to see the chip so we collapse it immediately.
    animation_->Hide();
  } else {
    prompt_bubble_ =
        new PermissionPromptBubbleView(browser_, delegate_, requested_time_);
    prompt_bubble_->Show();
    prompt_bubble_->GetWidget()->AddObserver(this);
    // Restart the timer after user clicks on the chip to open the bubble.
    StartCollapseTimer();
    if (!already_recorded_interaction_) {
      base::UmaHistogramLongTimes("Permissions.Chip.TimeToInteraction",
                                  base::TimeTicks::Now() - requested_time_);
      already_recorded_interaction_ = true;
    }
  }
  is_bubble_showing_ = !is_bubble_showing_;
}

void PermissionChip::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animation_.get());
  if (animation->GetCurrentValue() == 1.0)
    StartCollapseTimer();
}

void PermissionChip::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animation_.get());
  PreferredSizeChanged();
}

void PermissionChip::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
}

void PermissionChip::Collapse() {
  if (IsMouseHovered() || prompt_bubble_) {
    StartCollapseTimer();
  } else {
    animation_->Hide();
  }
}

void PermissionChip::StartCollapseTimer() {
  constexpr auto kDelayBeforeCollapsingChip =
      base::TimeDelta::FromMilliseconds(8000);
  timer_.Start(FROM_HERE, kDelayBeforeCollapsingChip, this,
               &PermissionChip::Collapse);
}

int PermissionChip::GetIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

void PermissionChip::UpdatePermissionIconAndTextColor() {
  if (!delegate_)
    return;

  // Set label and icon color to be the same color.
  SkColor enabled_text_color =
      views::style::GetColor(*chip_button_, views::style::CONTEXT_BUTTON_MD,
                             views::style::STYLE_DIALOG_BUTTON_DEFAULT);

  chip_button_->SetEnabledTextColors(enabled_text_color);
  chip_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(GetPermissionIconId(), GetIconSize(),
                            enabled_text_color));
}

const gfx::VectorIcon& PermissionChip::GetPermissionIconId() {
  auto requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconId();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->GetPermissionRequestType())
             ? requests[0]->GetIconId()
             : requests[1]->GetIconId();
}

base::string16 PermissionChip::GetPermissionMessage() {
  auto requests = delegate_->Requests();

  // TODO(olesiamarukhno): Update this to use real strings.
  return requests.size() == 1
             ? requests[0]->GetMessageTextFragment() + base::ASCIIToUTF16("?")
             : base::ASCIIToUTF16("Use camera & microphone?");
}
