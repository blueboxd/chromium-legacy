// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_popup_alignment_delegate.h"
#include "ash/system/message_center/message_center_ui_controller.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_tray_view.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/current_locale_view.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/managed_device_tray_item_view.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace ash {

class UnifiedSystemTray::UiDelegate : public MessageCenterUiDelegate {
 public:
  explicit UiDelegate(UnifiedSystemTray* owner);
  ~UiDelegate() override;

  // MessageCenterUiDelegate:
  void OnMessageCenterContentsChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter(bool show_by_click) override;
  void HideMessageCenter() override;

  MessageCenterUiController* ui_controller() { return ui_controller_.get(); }

  void SetTrayBubbleHeight(int height) {
    popup_alignment_delegate_->SetTrayBubbleHeight(height);
  }
  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id) {
    return message_popup_collection_->GetPopupViewForNotificationID(
        notification_id);
  }

 private:
  std::unique_ptr<MessageCenterUiController> ui_controller_;
  std::unique_ptr<AshPopupAlignmentDelegate> popup_alignment_delegate_;
  std::unique_ptr<message_center::MessagePopupCollection>
      message_popup_collection_;

  UnifiedSystemTray* const owner_;

  DISALLOW_COPY_AND_ASSIGN(UiDelegate);
};

const base::TimeDelta UnifiedSystemTray::kNotificationCountUpdateDelay =
    base::TimeDelta::FromMilliseconds(100);

UnifiedSystemTray::UiDelegate::UiDelegate(UnifiedSystemTray* owner)
    : owner_(owner) {
  ui_controller_ = std::make_unique<MessageCenterUiController>(this);
  ui_controller_->set_hide_on_last_notification(false);
  popup_alignment_delegate_ =
      std::make_unique<AshPopupAlignmentDelegate>(owner->shelf());
  message_popup_collection_ =
      std::make_unique<message_center::MessagePopupCollection>(
          popup_alignment_delegate_.get());
  message_popup_collection_->set_inverse();
  display::Screen* screen = display::Screen::GetScreen();
  popup_alignment_delegate_->StartObserving(
      screen, screen->GetDisplayNearestWindow(
                  owner->shelf()->GetStatusAreaWidget()->GetNativeWindow()));
}

UnifiedSystemTray::UiDelegate::~UiDelegate() = default;

void UnifiedSystemTray::UiDelegate::OnMessageCenterContentsChanged() {
  owner_->UpdateNotificationInternal();
}

bool UnifiedSystemTray::UiDelegate::ShowPopups() {
  if (owner_->IsBubbleShown())
    return false;
  return true;
}

void UnifiedSystemTray::UiDelegate::HidePopups() {
  popup_alignment_delegate_->SetTrayBubbleHeight(0);
}

bool UnifiedSystemTray::UiDelegate::ShowMessageCenter(bool show_by_click) {
  if (owner_->IsBubbleShown())
    return false;

  owner_->ShowBubbleInternal(show_by_click);
  return true;
}

void UnifiedSystemTray::UiDelegate::HideMessageCenter() {
  owner_->HideBubbleInternal();
}

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      ui_delegate_(std::make_unique<UiDelegate>(this)),
      model_(std::make_unique<UnifiedSystemTrayModel>()),
      slider_bubble_controller_(
          std::make_unique<UnifiedSliderBubbleController>(this)),
      current_locale_view_(new CurrentLocaleView(shelf)),
      ime_mode_view_(new ImeModeView(shelf)),
      managed_device_view_(new ManagedDeviceTrayItemView(shelf)),
      notification_counter_item_(new NotificationCounterView(shelf)),
      quiet_mode_view_(new QuietModeView(shelf)),
      time_view_(new tray::TimeTrayItemView(shelf)) {
  tray_container()->SetMargin(
      kUnifiedTrayContentPadding - TrayConstants::hit_region_padding(), 0);
  tray_container()->AddChildView(current_locale_view_);
  tray_container()->AddChildView(ime_mode_view_);
  tray_container()->AddChildView(managed_device_view_);
  tray_container()->AddChildView(notification_counter_item_);
  tray_container()->AddChildView(quiet_mode_view_);

  if (features::IsSeparateNetworkIconsEnabled()) {
    tray_container()->AddChildView(
        new tray::NetworkTrayView(shelf, ActiveNetworkIcon::Type::kPrimary));
    tray_container()->AddChildView(
        new tray::NetworkTrayView(shelf, ActiveNetworkIcon::Type::kCellular));
  } else {
    tray_container()->AddChildView(
        new tray::NetworkTrayView(shelf, ActiveNetworkIcon::Type::kSingle));
  }

  tray_container()->AddChildView(new tray::PowerTrayView(shelf));
  tray_container()->AddChildView(time_view_);

  SetInkDropMode(InkDropMode::ON);
  set_separator_visibility(false);
}

UnifiedSystemTray::~UnifiedSystemTray() {
  // Close bubble immediately when the bubble is closed on dtor.
  if (bubble_)
    bubble_->CloseNow();
  bubble_.reset();
}

bool UnifiedSystemTray::IsBubbleShown() const {
  return !!bubble_;
}

bool UnifiedSystemTray::IsSliderBubbleShown() const {
  return slider_bubble_controller_->IsBubbleShown();
}

bool UnifiedSystemTray::IsBubbleActive() const {
  return bubble_ && bubble_->IsBubbleActive();
}

void UnifiedSystemTray::ActivateBubble() {
  if (bubble_)
    bubble_->ActivateBubble();
}

void UnifiedSystemTray::EnsureBubbleExpanded() {
  if (bubble_)
    bubble_->EnsureExpanded();
}

void UnifiedSystemTray::ShowVolumeSliderBubble() {
  slider_bubble_controller_->ShowBubble(
      UnifiedSliderBubbleController::SLIDER_TYPE_VOLUME);
}

void UnifiedSystemTray::ShowAudioDetailedViewBubble() {
  // The settings menu bubble gains focus when |show_by_click| is true.
  ShowBubble(true /* show_by_click */);
  bubble_->ShowAudioDetailedView();
}

void UnifiedSystemTray::SetTrayBubbleHeight(int height) {
  ui_delegate_->SetTrayBubbleHeight(height);
}

gfx::Rect UnifiedSystemTray::GetBubbleBoundsInScreen() const {
  return bubble_ ? bubble_->GetBoundsInScreen() : gfx::Rect();
}

void UnifiedSystemTray::UpdateAfterLoginStatusChange() {
  SetVisible(true);
  PreferredSizeChanged();
}

bool UnifiedSystemTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback_enabled();
}

void UnifiedSystemTray::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  TrayBackgroundView::AddInkDropLayer(ink_drop_layer);
  ink_drop_layer_ = ink_drop_layer;
}

void UnifiedSystemTray::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  DCHECK_EQ(ink_drop_layer, ink_drop_layer_);
  TrayBackgroundView::RemoveInkDropLayer(ink_drop_layer);
  ink_drop_layer_ = nullptr;
}

void UnifiedSystemTray::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  TrayBackgroundView::OnBoundsChanged(previous_bounds);
  // Workarounding an ui::Layer bug that layer mask is not properly updated.
  // https://crbug.com/860367
  // TODO(tetsui): Remove after the bug is fixed on ui::Layer side.
  if (ink_drop_layer_) {
    ResetInkDropMask();
    InstallInkDropMask(ink_drop_layer_);
  }
}

const char* UnifiedSystemTray::GetClassName() const {
  return "UnifiedSystemTray";
}

void UnifiedSystemTray::SetTrayEnabled(bool enabled) {
  // We should close bubble at this point. If it remains opened and interactive,
  // it can be dangerous (http://crbug.com/497080).
  if (!enabled && bubble_)
    CloseBubble();

  SetEnabled(enabled);
}

void UnifiedSystemTray::SetTargetNotification(
    const std::string& notification_id) {
  model_->SetTargetNotification(notification_id);
}

bool UnifiedSystemTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    CloseBubble();
  } else {
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
    if (event.IsKeyEvent() || (event.flags() & ui::EF_TOUCH_ACCESSIBILITY))
      ActivateBubble();
  }
  return true;
}

void UnifiedSystemTray::ShowBubble(bool show_by_click) {
  // ShowBubbleInternal will be called from UiDelegate.
  if (!bubble_)
    ui_delegate_->ui_controller()->ShowMessageCenterBubble(show_by_click);
}

void UnifiedSystemTray::CloseBubble() {
  // HideBubbleInternal will be called from UiDelegate.
  ui_delegate_->ui_controller()->HideMessageCenterBubble();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForTray() {
  base::string16 time = base::TimeFormatTimeOfDayWithHourClockType(
      base::Time::Now(),
      Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
      base::kKeepAmPm);
  base::string16 battery = PowerStatus::Get()->GetAccessibleNameString(false);
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION,
                                    time, battery);
}

void UnifiedSystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void UnifiedSystemTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void UnifiedSystemTray::ClickedOutsideBubble() {
  CloseBubble();
}

void UnifiedSystemTray::UpdateAfterShelfAlignmentChange() {
  TrayBackgroundView::UpdateAfterShelfAlignmentChange();
  time_view_->UpdateAlignmentForShelf(shelf());
}

void UnifiedSystemTray::ShowBubbleInternal(bool show_by_click) {
  // Never show System Tray bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  // Hide volume/brightness slider popup.
  slider_bubble_controller_->CloseBubble();

  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this, show_by_click);
  SetIsActive(true);
}

void UnifiedSystemTray::HideBubbleInternal() {
  bubble_.reset();
  SetIsActive(false);
}

void UnifiedSystemTray::UpdateNotificationInternal() {
  // Limit update frequency in order to avoid flashing when 2 updates are
  // incoming in a very short period of time. It happens when ARC++ apps
  // creating bundled notifications.
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kNotificationCountUpdateDelay, this,
                 &UnifiedSystemTray::UpdateNotificationAfterDelay);
  }
}

void UnifiedSystemTray::UpdateNotificationAfterDelay() {
  notification_counter_item_->Update();
  quiet_mode_view_->Update();
}

message_center::MessagePopupView*
UnifiedSystemTray::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  return ui_delegate_->GetPopupViewForNotificationID(notification_id);
}

}  // namespace ash
