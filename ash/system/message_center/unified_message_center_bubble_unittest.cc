// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

class UnifiedMessageCenterBubbleTest : public AshTestBase {
 public:
  UnifiedMessageCenterBubbleTest() = default;
  ~UnifiedMessageCenterBubbleTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

 protected:
  std::string AddNotification() {
    std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, id,
        base::UTF8ToUTF16("test title"), base::UTF8ToUTF16("test message"),
        gfx::Image(), base::string16() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return id;
  }

  void EnableMessageCenterRefactor() {
    scoped_feature_list_->InitAndEnableFeature(
        features::kUnifiedMessageCenterRefactor);
  }

  UnifiedMessageCenterBubble* GetMessageCenterBubble() {
    return GetPrimaryUnifiedSystemTray()->message_center_bubble_for_test();
  }

  UnifiedSystemTrayBubble* GetSystemTrayBubble() {
    return GetPrimaryUnifiedSystemTray()->bubble();
  }

  int MessageCenterSeparationHeight() {
    gfx::Rect message_bubble_bounds =
        GetMessageCenterBubble()->GetBubbleView()->GetBoundsInScreen();
    gfx::Rect tray_bounds =
        GetSystemTrayBubble()->GetBubbleView()->GetBoundsInScreen();

    return message_bubble_bounds.y() + message_bubble_bounds.height() -
           tray_bounds.y();
  }

 private:
  int id_ = 0;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterBubbleTest);
};

TEST_F(UnifiedMessageCenterBubbleTest, VisibileOnlyWithNotifications) {
  EnableMessageCenterRefactor();
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
  // UnifiedMessageCenterBubble should not be visible when there are no
  // notifications.
  EXPECT_FALSE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());

  AddNotification();

  // UnifiedMessageCenterBubble should become visible after a notification is
  // added.
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
}

TEST_F(UnifiedMessageCenterBubbleTest, PositionedAboveSystemTray) {
  const int total_notifications = 5;
  EnableMessageCenterRefactor();
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
  AddNotification();

  const int reference_separation = MessageCenterSeparationHeight();

  // The message center should be positioned a constant distance above
  // the tray as it grows in size.
  for (int i = 0; i < total_notifications; i++) {
    AddNotification();
    EXPECT_EQ(reference_separation, MessageCenterSeparationHeight());
  }

  // When the system tray is collapsing, message view should stay at a constant
  // height above it.
  for (double i = 1.0; i >= 0; i -= 0.1) {
    GetSystemTrayBubble()->unified_view()->SetExpandedAmount(i);
    EXPECT_EQ(reference_separation, MessageCenterSeparationHeight());
  }

  // When the system tray is collapsing, message view should stay at a constant
  // height above it.
  for (double i = 0.0; i <= 1.0; i += 0.1) {
    GetSystemTrayBubble()->unified_view()->SetExpandedAmount(i);
    EXPECT_EQ(reference_separation, MessageCenterSeparationHeight());
  }
}

}  // namespace ash
