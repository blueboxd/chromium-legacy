// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/system/unified/unified_system_tray.h"
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
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        features::kUnifiedMessageCenterRefactor);
    AshTestBase::SetUp();
    tray_ = GetPrimaryUnifiedSystemTray();
    tray_->ShowBubble(true);
    message_center_bubble_ =
        std::make_unique<UnifiedMessageCenterBubble>(tray_);
  }

  void TearDown() override {
    // message_center_bubble_ needs to be destroyed before
    // the UnifiedSystemTray and TrayEventFilter. Need to do it
    // manually since we manually created it in SetUp.
    message_center_bubble_.reset();
    AshTestBase::TearDown();
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

  UnifiedSystemTray* tray() { return tray_; }

  UnifiedMessageCenterBubble* message_center_bubble() {
    return message_center_bubble_.get();
  }

 private:
  int id_ = 0;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  UnifiedSystemTray* tray_ = nullptr;
  std::unique_ptr<UnifiedMessageCenterBubble> message_center_bubble_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterBubbleTest);
};

TEST_F(UnifiedMessageCenterBubbleTest, VisibileOnlyWithNotifications) {
  // UnifiedMessageCenterBubble should not be visible when there are no
  // notifications.
  EXPECT_FALSE(message_center_bubble()->GetBubbleWidget()->IsVisible());

  AddNotification();

  // UnifiedMessageCenterBubble should become visible after a notification is
  // added.
  EXPECT_TRUE(message_center_bubble()->GetBubbleWidget()->IsVisible());
}

}  // namespace ash
