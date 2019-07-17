// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_view.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/rounded_label_button.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr base::TimeDelta kHideStackingBarAnimationDuration =
    base::TimeDelta::FromMilliseconds(330);
constexpr base::TimeDelta kCollapseAnimationDuration =
    base::TimeDelta::FromMilliseconds(640);

class ScrollerContentsView : public views::View {
 public:
  ScrollerContentsView(UnifiedMessageListView* message_list_view,
                       views::ButtonListener* listener) {
    auto* contents_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(0, 0, kUnifiedNotificationCenterSpacing, 0)));
    contents_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    AddChildView(message_list_view);
  }

  ~ScrollerContentsView() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* view) override {
    PreferredSizeChanged();
  }

  const char* GetClassName() const override { return "ScrollerContentsView"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollerContentsView);
};

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
        GetInkDropCenterBasedOnLastEvent(), SK_ColorBLACK);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(
        TrayPopupInkDropStyle::FILL_BOUNDS, this, SK_ColorBLACK);
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
  DISALLOW_COPY_AND_ASSIGN(StackingBarClearAllButton);
};

}  // namespace

StackingNotificationCounterView::StackingNotificationCounterView(
    views::ButtonListener* listener)
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

  count_label_->SetEnabledColor(kStackingNotificationCounterLabelColor);
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

StackingNotificationCounterView::~StackingNotificationCounterView() = default;

bool StackingNotificationCounterView::SetCount(int total_notification_count,
                                               int stacked_notification_count) {
  if (total_notification_count == total_notification_count_ &&
      stacked_notification_count == stacked_notification_count_)
    return false;

  total_notification_count_ = total_notification_count;
  stacked_notification_count_ = stacked_notification_count;

  UpdateVisibility();

  auto tooltip = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_STACKING_BAR_CLEAR_ALL_BUTTON_TOOLTIP,
      total_notification_count_);
  clear_all_button_->SetTooltipText(tooltip);
  clear_all_button_->SetAccessibleName(tooltip);

  if (stacked_notification_count_ > 0) {
    count_label_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_ASH_MESSAGE_CENTER_HIDDEN_NOTIFICATION_COUNT_LABEL,
        stacked_notification_count_));
    count_label_->SetVisible(true);
  } else {
    count_label_->SetVisible(false);
  }

  SchedulePaint();
  return true;
}

void StackingNotificationCounterView::SetAnimationState(
    UnifiedMessageCenterAnimationState animation_state) {
  animation_state_ = animation_state;
  UpdateVisibility();
}

void StackingNotificationCounterView::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColor(message_center::kNotificationBackgroundColor);
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
      kStackingNotificationCounterBorderColor);
}

const char* StackingNotificationCounterView::GetClassName() const {
  return "StackingNotificationCounterView";
}

void StackingNotificationCounterView::UpdateVisibility() {
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

UnifiedMessageCenterView::UnifiedMessageCenterView(
    UnifiedSystemTrayView* parent,
    UnifiedSystemTrayModel* model)
    : parent_(parent),
      model_(model),
      stacking_counter_(new StackingNotificationCounterView(this)),
      scroll_bar_(new MessageCenterScrollBar(this)),
      scroller_(new views::ScrollView()),
      message_list_view_(new UnifiedMessageListView(this, model)),
      last_scroll_position_from_bottom_(0),
      animation_(std::make_unique<gfx::LinearAnimation>(this)) {
  message_list_view_->Init();

  AddChildView(stacking_counter_);

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetContents(
      std::make_unique<ScrollerContentsView>(message_list_view_, this));
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(scroll_bar_);
  scroller_->SetDrawOverflowIndicator(false);
  AddChildView(scroller_);
}

UnifiedMessageCenterView::~UnifiedMessageCenterView() {
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);

  RemovedFromWidget();
}

void UnifiedMessageCenterView::SetMaxHeight(int max_height) {
  scroller_->ClipHeightTo(0, max_height);
}

void UnifiedMessageCenterView::SetAvailableHeight(int available_height) {
  available_height_ = available_height;
  UpdateVisibility();
}

void UnifiedMessageCenterView::OnNotificationSlidOut() {
  if (stacking_counter_->GetVisible() &&
      message_list_view_->GetTotalNotificationCount() <= 1) {
    StartHideStackingBarAnimation();
  } else if (!message_list_view_->GetTotalNotificationCount()) {
    StartCollapseAnimation();
  }
}

void UnifiedMessageCenterView::ListPreferredSizeChanged() {
  UpdateVisibility();
  PreferredSizeChanged();
  Layout();

  if (GetWidget() && !GetWidget()->IsClosed())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void UnifiedMessageCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void UnifiedMessageCenterView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void UnifiedMessageCenterView::RemovedFromWidget() {
  if (!focus_manager_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void UnifiedMessageCenterView::Layout() {
  stacking_counter_->SetCount(message_list_view_->GetTotalNotificationCount(),
                              GetStackedNotificationCount());
  if (stacking_counter_->GetVisible()) {
    gfx::Rect counter_bounds(GetContentsBounds());

    int stacking_counter_height = kStackingNotificationCounterHeight;
    int stacking_counter_offset = 0;
    if (animation_state_ ==
        UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR)
      stacking_counter_offset = GetAnimationValue() * stacking_counter_height;

    counter_bounds.set_height(stacking_counter_height);
    counter_bounds.set_y(counter_bounds.y() - stacking_counter_offset);
    stacking_counter_->SetBoundsRect(counter_bounds);

    gfx::Rect scroller_bounds(GetContentsBounds());
    scroller_bounds.Inset(gfx::Insets(
        stacking_counter_height - stacking_counter_offset, 0, 0, 0));
    scroller_->SetBoundsRect(scroller_bounds);
  } else {
    scroller_->SetBoundsRect(GetContentsBounds());
  }

  ScrollToTarget();
  NotifyRectBelowScroll();
}

gfx::Size UnifiedMessageCenterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = scroller_->GetPreferredSize();

  if (stacking_counter_->GetVisible()) {
    int bar_height = kStackingNotificationCounterHeight;
    if (animation_state_ ==
        UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR)
      bar_height -= GetAnimationValue() * bar_height;
    preferred_size.set_height(preferred_size.height() + bar_height);
  }

  if (animation_state_ == UnifiedMessageCenterAnimationState::COLLAPSE) {
    int height = preferred_size.height() * (1.0 - GetAnimationValue());
    preferred_size.set_height(height);
  }

  return preferred_size;
}

const char* UnifiedMessageCenterView::GetClassName() const {
  return "UnifiedMessageCenterView";
}

void UnifiedMessageCenterView::OnMessageCenterScrolled() {
  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();

  // Reset the target if user scrolls the list manually.
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION);

  bool was_count_updated = stacking_counter_->SetCount(
      message_list_view_->GetTotalNotificationCount(),
      GetStackedNotificationCount());
  if (was_count_updated) {
    const int previous_y = scroller_->y();
    Layout();
    // Adjust scroll position when counter visibility is changed so that
    // on-screen position of notification list does not change.
    scroll_bar_->ScrollByContentsOffset(previous_y - scroller_->y());
  }

  NotifyRectBelowScroll();
}

void UnifiedMessageCenterView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_StackingBarClearAll"));

  message_list_view_->ClearAllWithAnimation();
}

void UnifiedMessageCenterView::OnWillChangeFocus(views::View* before,
                                                 views::View* now) {}

void UnifiedMessageCenterView::OnDidChangeFocus(views::View* before,
                                                views::View* now) {
  if (message_list_view_->is_deleting_removed_notifications())
    return;

  OnMessageCenterScrolled();
}

void UnifiedMessageCenterView::AnimationEnded(const gfx::Animation* animation) {
  // This is also called from AnimationCanceled().
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  switch (animation_state_) {
    case UnifiedMessageCenterAnimationState::IDLE:
      break;
    case UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR:
      break;
    case UnifiedMessageCenterAnimationState::COLLAPSE:
      break;
  }

  animation_state_ = UnifiedMessageCenterAnimationState::IDLE;
  stacking_counter_->SetAnimationState(animation_state_);
  UpdateVisibility();
}

void UnifiedMessageCenterView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void UnifiedMessageCenterView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void UnifiedMessageCenterView::SetNotificationRectBelowScroll(
    const gfx::Rect& rect_below_scroll) {
  parent_->SetNotificationRectBelowScroll(rect_below_scroll);
}

void UnifiedMessageCenterView::StartHideStackingBarAnimation() {
  animation_->End();
  animation_state_ = UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR;
  stacking_counter_->SetAnimationState(animation_state_);
  animation_->SetDuration(kHideStackingBarAnimationDuration);
  animation_->Start();
}

void UnifiedMessageCenterView::StartCollapseAnimation() {
  animation_->End();
  animation_state_ = UnifiedMessageCenterAnimationState::COLLAPSE;
  stacking_counter_->SetAnimationState(animation_state_);
  animation_->SetDuration(kCollapseAnimationDuration);
  animation_->Start();
}

double UnifiedMessageCenterView::GetAnimationValue() const {
  return gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                    animation_->GetCurrentValue());
}

void UnifiedMessageCenterView::UpdateVisibility() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  SetVisible(
      available_height_ >= kUnifiedNotificationMinimumHeight &&
      (animation_state_ == UnifiedMessageCenterAnimationState::COLLAPSE ||
       message_list_view_->GetPreferredSize().height() > 0) &&
      session_controller->ShouldShowNotificationTray() &&
      (!session_controller->IsScreenLocked() ||
       AshMessageCenterLockScreenController::IsEnabled()));

  // When notification list went invisible, the last notification should be
  // targeted next time.
  if (!GetVisible()) {
    model_->set_notification_target_mode(
        UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);
    NotifyRectBelowScroll();
  }
}

void UnifiedMessageCenterView::ScrollToTarget() {
  // Following logic doesn't work when the view is invisible, because it uses
  // the height of |scroller_|.
  if (!GetVisible())
    return;

  auto target_mode = model_->notification_target_mode();

  // Notification views may be deleted during an animation, so wait until it
  // finishes before scrolling to a new target (see crbug.com/954001).
  if (message_list_view_->IsAnimating())
    target_mode = UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION;

  int position;
  switch (target_mode) {
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION:
      // Restore the previous scrolled position with matching the distance from
      // the bottom.
      position =
          scroll_bar_->GetMaxPosition() - last_scroll_position_from_bottom_;
      break;
    case UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID:
      FALLTHROUGH;
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION: {
      const gfx::Rect& target_rect =
          (model_->notification_target_mode() ==
           UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID)
              ? message_list_view_->GetNotificationBounds(
                    model_->notification_target_id())
              : message_list_view_->GetLastNotificationBounds();

      const int last_notification_offset = target_rect.height() -
                                           scroller_->height() +
                                           kUnifiedNotificationCenterSpacing;
      if (last_notification_offset > 0) {
        // If the target notification is taller than |scroller_|, we should
        // align the top of the notification with the top of |scroller_|.
        position = target_rect.y();
      } else {
        // Otherwise, we align the bottom of the notification with the bottom of
        // |scroller_|;
        position = target_rect.bottom() - scroller_->height();

        if (model_->notification_target_mode() ==
            UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION) {
          position += kUnifiedNotificationCenterSpacing;
        }
      }
    }
  }

  scroller_->ScrollToPosition(scroll_bar_, position);
  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();
}

int UnifiedMessageCenterView::GetStackedNotificationCount() const {
  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty())
    scroller_->SetBoundsRect(GetContentsBounds());

  // Consistently use the y offset below the stacked notification bar in the
  // UnifiedMessageCenterView to count number of hidden notifications.
  const int y_offset = scroller_->GetVisibleRect().y() - scroller_->y() +
                       kStackingNotificationCounterHeight;
  return message_list_view_->CountNotificationsAboveY(y_offset);
}

void UnifiedMessageCenterView::NotifyRectBelowScroll() {
  // If the message center is hidden, make sure rounded corners are not drawn.
  if (!GetVisible()) {
    SetNotificationRectBelowScroll(gfx::Rect());
    return;
  }

  gfx::Rect rect_below_scroll;
  rect_below_scroll.set_height(
      std::max(0, message_list_view_->GetLastNotificationBounds().bottom() -
                      scroller_->GetVisibleRect().bottom()));

  gfx::Rect notification_bounds =
      message_list_view_->GetNotificationBoundsBelowY(
          scroller_->GetVisibleRect().bottom());
  rect_below_scroll.set_x(notification_bounds.x());
  rect_below_scroll.set_width(notification_bounds.width());

  SetNotificationRectBelowScroll(rect_below_scroll);
}

}  // namespace ash
