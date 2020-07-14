// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
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

  ShareTargetBuilder& set_is_incoming(bool is_incoming) {
    is_incoming_ = is_incoming;
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
                       text_attachments_, file_attachments_, is_incoming_,
                       /*full_name=*/base::nullopt,
                       /*is_known=*/false);
  }

 private:
  std::string device_name_;
  bool is_incoming_ = false;
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

TextAttachment CreateTextAttachment(TextAttachment::Type type) {
  return TextAttachment("text body", type, /*size=*/9);
}

FileAttachment CreateFileAttachment(FileAttachment::Type type) {
  return FileAttachment(/*file_name=*/"file.jpg", type,
                        /*size=*/10,
                        /*file_path=*/base::nullopt,
                        /*mime_type=*/"example");
}

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

struct ProgressNotificationTestParamInternal {
  std::vector<TextAttachment::Type> text_attachments;
  std::vector<FileAttachment::Type> file_attachments;
  int expected_resource_id;
};

ProgressNotificationTestParamInternal kProgressNotificationTestParams[] = {
    // No attachments.
    {{}, {}, IDS_NEARBY_UNKNOWN_ATTACHMENTS},

    // Mixed attachments.
    {{TextAttachment::Type::kText},
     {FileAttachment::Type::kUnknown},
     IDS_NEARBY_UNKNOWN_ATTACHMENTS},

    // Text attachments.
    {{TextAttachment::Type::kUrl}, {}, IDS_NEARBY_TEXT_ATTACHMENTS_LINKS},
    {{TextAttachment::Type::kText}, {}, IDS_NEARBY_TEXT_ATTACHMENTS_UNKNOWN},
    {{TextAttachment::Type::kAddress},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_ADDRESSES},
    {{TextAttachment::Type::kPhoneNumber},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_PHONE_NUMBERS},
    {{TextAttachment::Type::kAddress, TextAttachment::Type::kAddress},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_ADDRESSES},
    {{TextAttachment::Type::kAddress, TextAttachment::Type::kUrl},
     {},
     IDS_NEARBY_TEXT_ATTACHMENTS_UNKNOWN},

    // File attachments.
    {{}, {FileAttachment::Type::kApp}, IDS_NEARBY_FILE_ATTACHMENTS_APPS},
    {{}, {FileAttachment::Type::kImage}, IDS_NEARBY_FILE_ATTACHMENTS_IMAGES},
    {{}, {FileAttachment::Type::kUnknown}, IDS_NEARBY_FILE_ATTACHMENTS_UNKNOWN},
    {{}, {FileAttachment::Type::kVideo}, IDS_NEARBY_FILE_ATTACHMENTS_VIDEOS},
    {{},
     {FileAttachment::Type::kApp, FileAttachment::Type::kApp},
     IDS_NEARBY_FILE_ATTACHMENTS_APPS},
    {{},
     {FileAttachment::Type::kApp, FileAttachment::Type::kImage},
     IDS_NEARBY_FILE_ATTACHMENTS_UNKNOWN},
};

using ProgressNotificationTestParam =
    std::tuple<ProgressNotificationTestParamInternal, bool>;

class NearbyNotificationManagerProgressNotificationTest
    : public NearbyNotificationManagerTest,
      public testing::WithParamInterface<ProgressNotificationTestParam> {};

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
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());

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

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
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

TEST_P(NearbyNotificationManagerProgressNotificationTest, Test) {
  const ProgressNotificationTestParamInternal& param = std::get<0>(GetParam());
  bool is_incoming = std::get<1>(GetParam());

  std::string device_name = "device";
  ShareTargetBuilder share_target_builder;
  share_target_builder.set_device_name(device_name);
  share_target_builder.set_is_incoming(is_incoming);

  for (TextAttachment::Type type : param.text_attachments)
    share_target_builder.add_attachment(CreateTextAttachment(type));

  for (FileAttachment::Type type : param.file_attachments)
    share_target_builder.add_attachment(CreateFileAttachment(type));

  ShareTarget share_target = share_target_builder.build();
  TransferMetadata transfer_metadata = TransferMetadataBuilder().build();
  manager()->ShowProgress(share_target, transfer_metadata);

  int expected_resource_id =
      is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE
                  : IDS_NEARBY_NOTIFICATION_SEND_PROGRESS_TITLE;
  size_t total = param.text_attachments.size() + param.file_attachments.size();
  base::string16 expected = l10n_util::GetStringFUTF16(
      expected_resource_id,
      l10n_util::GetPluralStringFUTF16(param.expected_resource_id, total),
      base::ASCIIToUTF16(device_name));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(expected, notification.title());
}

INSTANTIATE_TEST_SUITE_P(
    NearbyNotificationManagerProgressNotificationTest,
    NearbyNotificationManagerProgressNotificationTest,
    testing::Combine(testing::ValuesIn(kProgressNotificationTestParams),
                     testing::Bool()));

TEST_F(NearbyNotificationManagerTest, ShowConnectionRequest_ShowsNotification) {
  std::string device_name = "device";
  ShareTargetBuilder share_target_builder;
  share_target_builder.set_device_name(device_name);
  share_target_builder.add_attachment(
      CreateFileAttachment(FileAttachment::Type::kImage));
  ShareTarget share_target = share_target_builder.build();

  manager()->ShowConnectionRequest(share_target);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];

  base::string16 expected_title = l10n_util::GetStringUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE);

  base::string16 expected_message = l10n_util::GetStringFUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE,
      base::ASCIIToUTF16(device_name),
      l10n_util::GetPluralStringFUTF16(IDS_NEARBY_FILE_ATTACHMENTS_IMAGES, 1));

  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(expected_title, notification.title());
  EXPECT_EQ(expected_message, notification.message());
  // TODO(crbug.com/1102348): verify notification.icon()
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());

  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  ASSERT_EQ(2u, buttons.size());

  const message_center::ButtonInfo& receive_button = buttons[0];
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_RECEIVE_ACTION),
            receive_button.title);
  const message_center::ButtonInfo& decline_button = buttons[1];
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION),
            decline_button.title);
}

TEST_F(NearbyNotificationManagerTest, ShowOnboarding_ShowsNotification) {
  manager()->ShowOnboarding();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE),
            notification.title());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE),
      notification.message());
  EXPECT_TRUE(notification.icon().IsEmpty());
  EXPECT_EQ(GURL(), notification.origin_url());
  EXPECT_FALSE(notification.never_timeout());
  EXPECT_FALSE(notification.renotify());
  EXPECT_EQ(&kNearbyShareIcon, &notification.vector_small_image());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
            notification.display_source());
  EXPECT_EQ(0u, notification.buttons().size());
}
