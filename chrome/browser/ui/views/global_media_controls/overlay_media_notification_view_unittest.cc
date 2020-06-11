// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/overlay_media_notification_view.h"

#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const char kTestNotificationId[] = "testid";

}  // anonymous namespace

class MockOverlayMediaNotificationsManager final
    : public OverlayMediaNotificationsManager {
 public:
  MockOverlayMediaNotificationsManager() = default;
  ~MockOverlayMediaNotificationsManager() = default;

  MOCK_METHOD1(OnOverlayNotificationClosed, void(const std::string& id));
};

class OverlayMediaNotificationViewTest : public ChromeViewsTestBase {
 public:
  OverlayMediaNotificationViewTest() = default;
  ~OverlayMediaNotificationViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    manager_ = std::make_unique<MockOverlayMediaNotificationsManager>();

    auto notification = std::make_unique<MediaNotificationContainerImplView>(
        kTestNotificationId, nullptr);

    overlay_ = std::make_unique<OverlayMediaNotificationView>(
        kTestNotificationId, std::move(notification),
        gfx::Rect(notification->GetPreferredSize()), GetContext());

    overlay_->SetManager(manager_.get());
    overlay_->ShowNotification();
  }

  void TearDown() override {
    overlay_.reset();
    manager_.reset();
    ViewsTestBase::TearDown();
  }

  void SimulateTitleChange(const base::string16 title) {
    media_session::MediaMetadata metadata;
    metadata.source_title = base::ASCIIToUTF16("source_title");
    metadata.title = title;
    metadata.artist = base::ASCIIToUTF16("artist");
    GetView()->UpdateWithMediaMetadata(metadata);
  }

  void SimulateExpandStateChanged(bool expand) {
    overlay_->notification_for_testing()->OnExpanded(expand);
  }

  base::string16 GetWindowTitle() {
    return overlay_->widget_delegate()->GetWindowTitle();
  }

  gfx::Size GetWindowSize() {
    return overlay_->GetWindowBoundsInScreen().size();
  }

 private:
  media_message_center::MediaNotificationViewImpl* GetView() {
    return overlay_->notification_for_testing()->view_for_testing();
  }

  std::unique_ptr<MockOverlayMediaNotificationsManager> manager_ = nullptr;

  std::unique_ptr<OverlayMediaNotificationView> overlay_ = nullptr;
};

TEST_F(OverlayMediaNotificationViewTest, TaskBarTitle) {
  base::string16 title1 = base::ASCIIToUTF16("test");
  SimulateTitleChange(title1);
  EXPECT_EQ(GetWindowTitle(), title1);

  base::string16 title2 = base::ASCIIToUTF16("title");
  SimulateTitleChange(title2);
  EXPECT_EQ(GetWindowTitle(), title2);
}

TEST_F(OverlayMediaNotificationViewTest, ResizeOnExpandStateChanged) {
  constexpr int kExpandedHeight = 150;
  constexpr int kNormalHeight = 100;

  EXPECT_EQ(kNormalHeight, GetWindowSize().height());

  SimulateExpandStateChanged(true);
  EXPECT_EQ(kExpandedHeight, GetWindowSize().height());

  SimulateExpandStateChanged(false);
  EXPECT_EQ(kNormalHeight, GetWindowSize().height());
}
