// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/stacked_notification_bar.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/rounded_label_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The "Clear all" button in the stacking notification bar.
class StackingBarClearAllButton : public views::LabelButton {
 public:
  StackingBarClearAllButton(views::ButtonListener* listener,
                            const base::string16& text)
      : views::LabelButton(listener, text) {
    SetEnabledTextColors(kUnifiedMenuButtonColorActive);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    label()->SetSubpixelRenderingEnabled(false);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    TrayPopupUtils::ConfigureTrayPopupButton(this);

    background_color_ = AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparentWithoutBlur,
        kNotificationBackgroundColor);
  }

  ~StackingBarClearAllButton() override = default;

  // views::LabelButton:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(label()->GetPreferredSize().width() +
                         kStackingNotificationClearAllButtonPadding.width(),
                     label()->GetPreferredSize().height() +
                         kStackingNotificationClearAllButtonPadding.height());
  }

  const char* GetClassName() const override {
    return "StackingBarClearAllButton";
  }

  int GetHeightForWidth(int width) const override {
    return label()->GetPreferredSize().height() +
           kStackingNotificationClearAllButtonPadding.height();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::LabelButton::PaintButtonContents(canvas);
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = TrayPopupUtils::CreateInkDrop(this);
    ink_drop->SetShowHighlightOnFocus(true);
    ink_drop->SetShowHighlightOnHover(true);
    return ink_drop;
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return TrayPopupUtils::CreateInkDropRipple(
        TrayPopupInkDropStyle::FILL_BOUNDS, this,
        GetInkDropCenterBasedOnLastEvent(), background_color_);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(
        TrayPopupInkDropStyle::FILL_BOUNDS, this, background_color_);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    SkScalar top_radius = SkIntToScalar(kUnifiedTrayCornerRadius);
    SkRect bounds = gfx::RectToSkRect(GetContentsBounds());
    SkPath path;

    if (base::i18n::IsRTL()) {
      SkScalar radii[8] = {top_radius, top_radius, 0, 0, 0, 0, 0, 0};
      path.addRoundRect(bounds, radii);
    } else {
      SkScalar radii[8] = {0, 0, top_radius, top_radius, 0, 0, 0, 0};
      path.addRoundRect(bounds, radii);
    }

    return std::make_unique<views::PathInkDropMask>(size(), path);
  }

 private:
  SkColor background_color_ = gfx::kPlaceholderColor;

  DISALLOW_COPY_AND_ASSIGN(StackingBarClearAllButton);
};

}  // namespace

StackedNotificationBar::StackedNotificationBar(views::ButtonListener* listener)
    : count_label_(new views::Label),
      clear_all_button_(new StackingBarClearAllButton(
          listener,
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_LABEL))) {
  SetVisible(false);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kStackingNotificationClearAllButtonPadding.left(), 0, 0),
      0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  if (features::IsUnifiedMessageCenterRefactorEnabled()) {
    notification_icons_container_ = new views::View();
    notification_icons_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            kStackedNotificationIconsContainerPadding));
    AddChildView(notification_icons_container_);
  }

  count_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextSecondary,
      AshColorProvider::AshColorMode::kLight));
  count_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  AddChildView(count_label_);

  views::View* spacer = new views::View;
  AddChildView(spacer);
  layout->SetFlexForView(spacer, 1);

  clear_all_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
  AddChildView(clear_all_button_);
}

StackedNotificationBar::~StackedNotificationBar() = default;

bool StackedNotificationBar::Update(
    int total_notification_count,
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();

  if (total_notification_count == total_notification_count_ &&
      stacked_notification_count == stacked_notification_count_)
    return false;

  total_notification_count_ = total_notification_count;

  UpdateVisibility();

  auto tooltip = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_STACKING_BAR_CLEAR_ALL_BUTTON_TOOLTIP,
      total_notification_count_);
  clear_all_button_->SetTooltipText(tooltip);
  clear_all_button_->SetAccessibleName(tooltip);

  UpdateStackedNotifications(stacked_notifications);

  return true;
}

void StackedNotificationBar::SetAnimationState(
    UnifiedMessageCenterAnimationState animation_state) {
  animation_state_ = animation_state;
  UpdateVisibility();
}

void StackedNotificationBar::AddNotificationIcon(
    message_center::Notification* notification,
    bool at_front) {
  SkColor accent_color = notification->accent_color() == SK_ColorTRANSPARENT
                             ? message_center::kNotificationDefaultAccentColor
                             : notification->accent_color();

  gfx::Image masked_small_icon =
      notification->GenerateMaskedSmallIcon(15, accent_color);

  views::ImageView* icon_view_ = new views::ImageView();

  if (at_front)
    notification_icons_container_->AddChildViewAt(icon_view_, 0);
  else
    notification_icons_container_->AddChildView(icon_view_);

  if (masked_small_icon.IsEmpty()) {
    icon_view_->SetImage(gfx::CreateVectorIcon(message_center::kProductIcon,
                                               kStackedNotificationIconSize,
                                               accent_color));
  } else {
    icon_view_->SetImage(masked_small_icon.AsImageSkia());
  }
}

void StackedNotificationBar::ShiftIconsLeft(
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();
  int removed_icons_count =
      std::min(stacked_notification_count_ - stacked_notification_count,
               kStackedNotificationBarMaxIcons);

  // Remove required number of icons from the front.
  for (int i = 0; i < removed_icons_count; i++) {
    delete notification_icons_container_->children().front();
  }

  // Add icons to the back if there was a backfill.
  int backfill_start = kStackedNotificationBarMaxIcons - removed_icons_count;
  int backfill_end =
      std::min(kStackedNotificationBarMaxIcons, stacked_notification_count);
  for (int i = backfill_start; i < backfill_end; i++) {
    AddNotificationIcon(stacked_notifications[i], false /*at_front*/);
  }

  stacked_notification_count_ = stacked_notification_count;
}

void StackedNotificationBar::ShiftIconsRight(
    std::vector<message_center::Notification*> stacked_notifications) {
  int new_stacked_notification_count = stacked_notifications.size();

  while (stacked_notification_count_ < new_stacked_notification_count) {
    // Remove icon from the back in case there is an overflow.
    if (stacked_notification_count_ >= kStackedNotificationBarMaxIcons) {
      delete notification_icons_container_->children().back();
    }
    // Add icon to the front.
    AddNotificationIcon(stacked_notifications[new_stacked_notification_count -
                                              stacked_notification_count_ - 1],
                        true /*at_front*/);
    ++stacked_notification_count_;
  }
}

void StackedNotificationBar::UpdateStackedNotifications(
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();
  int notification_overflow_count = 0;

  if (features::IsUnifiedMessageCenterRefactorEnabled()) {
    if (stacked_notification_count_ > stacked_notification_count)
      ShiftIconsLeft(stacked_notifications);
    else if (stacked_notification_count_ < stacked_notification_count)
      ShiftIconsRight(stacked_notifications);

    notification_overflow_count = std::max(
        stacked_notification_count_ - kStackedNotificationBarMaxIcons, 0);
  } else {
    stacked_notification_count_ = stacked_notification_count;
    notification_overflow_count = stacked_notification_count;
  }

  // Update overflow count label
  if (notification_overflow_count > 0) {
    count_label_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_ASH_MESSAGE_CENTER_HIDDEN_NOTIFICATION_COUNT_LABEL,
        notification_overflow_count));
    count_label_->SetVisible(true);
  } else {
    count_label_->SetVisible(false);
  }

  Layout();
}

void StackedNotificationBar::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColor(AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparentWithoutBlur,
      kNotificationBackgroundColor));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  SkPath background_path;
  SkScalar top_radius = SkIntToScalar(kUnifiedTrayCornerRadius);
  SkScalar radii[8] = {top_radius, top_radius, top_radius, top_radius,
                       0,          0,          0,          0};

  gfx::Rect bounds = GetLocalBounds();
  background_path.addRoundRect(gfx::RectToSkRect(bounds), radii);
  canvas->DrawPath(background_path, flags);

  // We draw a border here than use a views::Border so the ink drop highlight
  // of the clear all button overlays the border.
  canvas->DrawSharpLine(
      gfx::PointF(bounds.bottom_left() - gfx::Vector2d(0, 1)),
      gfx::PointF(bounds.bottom_right() - gfx::Vector2d(0, 1)),
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kSeparator,
          AshColorProvider::AshColorMode::kLight));
}

const char* StackedNotificationBar::GetClassName() const {
  return "StackedNotificationBar";
}

void StackedNotificationBar::UpdateVisibility() {
  switch (animation_state_) {
    case UnifiedMessageCenterAnimationState::IDLE:
      SetVisible(total_notification_count_ > 1);
      break;
    case UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR:
      SetVisible(true);
      break;
    case UnifiedMessageCenterAnimationState::COLLAPSE:
      SetVisible(false);
      break;
  }
}

}  // namespace ash