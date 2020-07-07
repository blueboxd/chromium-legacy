// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

class ShareTargetBuilder {
 public:
  ShareTargetBuilder() = default;
  ~ShareTargetBuilder() = default;

  ShareTargetBuilder& set_device_name(const std::string& device_name) {
    device_name_ = device_name;
    return *this;
  }

  ShareTargetBuilder& add_attachment(TextAttachment attachment) {
    text_attachments_.push_back(std::move(attachment));
    return *this;
  }

  ShareTargetBuilder& add_attachment(FileAttachment attachment) {
    file_attachments_.push_back(std::move(attachment));
    return *this;
  }

  ShareTarget build() const {
    return ShareTarget(device_name_,
                       /*image_url=*/GURL(), ShareTarget::Type::kPhone,
                       text_attachments_, file_attachments_,
                       /*is_incoming=*/false,
                       /*full_name=*/base::nullopt,
                       /*is_known=*/false);
  }

 private:
  std::string device_name_;
  std::vector<TextAttachment> text_attachments_;
  std::vector<FileAttachment> file_attachments_;
};

class TransferMetadataBuilder {
 public:
  TransferMetadataBuilder() = default;
  ~TransferMetadataBuilder() = default;

  TransferMetadataBuilder& set_progress(double progress) {
    progress_ = progress;
    return *this;
  }

  TransferMetadata build() const {
    return TransferMetadata(TransferMetadata::Status::kInProgress, progress_,
                            /*token=*/base::nullopt,
                            /*is_original=*/false,
                            /*is_final_status=*/false);
  }

 private:
  double progress_ = 0;
};

class NearbyNotificationManagerTest : public testing::Test {
 public:
  NearbyNotificationManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNearbySharing);
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);
  }
  ~NearbyNotificationManagerTest() override = default;

  NearbyNotificationManager* manager() { return &manager_; }

  std::vector<message_center::Notification> GetDisplayedNotifications() {
    return notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::NEARBY_SHARE);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  NearbyNotificationManager manager_{&profile_};
};

}  // namespace

TEST_F(NearbyNotificationManagerTest, ShowProgress_ShowsNotification) {
  ShareTarget share_target = ShareTargetBuilder().build();
  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();

  manager()->ShowProgress(share_target, transfer_metadata);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification.type());
  EXPECT_EQ(base::string16(), notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(1u, buttons.size());

  const message_center::ButtonInfo& cancel_button = buttons[0];
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), cancel_button.title);
}

TEST_F(NearbyNotificationManagerTest, ShowProgress_ShowsProgress) {
  double progress = 0.75;

  ShareTarget share_target = ShareTargetBuilder().build();
  TransferMetadata transfer_metadata =
      TransferMetadataBuilder().set_progress(progress).build();

  manager()->ShowProgress(share_target, transfer_metadata);

  message_center::Notification notification = GetDisplayedNotifications()[0];
  EXPECT_EQ(100.0 * progress, notification.progress());
}

TEST_F(NearbyNotificationManagerTest, ShowProgress_UpdatesProgress) {
  ShareTarget share_target = ShareTargetBuilder().build();
  TransferMetadataBuilder transfer_metadata_builder;
  transfer_metadata_builder.set_progress(0.75);

  manager()->ShowProgress(share_target, transfer_metadata_builder.build());

  double progress = 0.5;
  transfer_metadata_builder.set_progress(progress);
  manager()->ShowProgress(share_target, transfer_metadata_builder.build());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(100.0 * progress, notification.progress());
}
