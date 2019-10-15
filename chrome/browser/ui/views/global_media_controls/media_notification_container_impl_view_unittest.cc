// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using ::testing::_;

namespace {

const char kTestNotificationId[] = "testid";
const char kOtherTestNotificationId[] = "othertestid";

class MockMediaNotificationContainerObserver
    : public MediaNotificationContainerObserver {
 public:
  MockMediaNotificationContainerObserver() = default;
  ~MockMediaNotificationContainerObserver() = default;

  // MediaNotificationContainerObserver implementation.
  MOCK_METHOD1(OnContainerExpanded, void(bool expanded));
  MOCK_METHOD0(OnContainerMetadataChanged, void());
  MOCK_METHOD1(OnContainerDismissed, void(const std::string& id));
  MOCK_METHOD1(OnContainerDestroyed, void(const std::string& id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationContainerObserver);
};

}  // anonymous namespace

class MediaNotificationContainerImplViewTest : public views::ViewsTestBase {
 public:
  MediaNotificationContainerImplViewTest() = default;
  ~MediaNotificationContainerImplViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(400, 300);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));
    views::View* container_view = new views::View();
    widget_.SetContentsView(container_view);

    auto notification_container =
        std::make_unique<MediaNotificationContainerImplView>(
            kTestNotificationId, nullptr);
    notification_container_ =
        container_view->AddChildView(std::move(notification_container));

    observer_ = std::make_unique<MockMediaNotificationContainerObserver>();
    notification_container_->AddObserver(observer_.get());

    SimulateMediaSessionData();

    widget_.Show();
  }

  void TearDown() override {
    notification_container_->RemoveObserver(observer_.get());
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  void SimulateNotificationSwipedToDismiss() {
    // When the notification is swiped, the SlideOutController sends this to the
    // MediaNotificationContainerImplView.
    notification_container()->OnSlideOut();
  }

  bool IsDismissButtonVisible() { return GetDismissButton()->IsDrawn(); }

  void SimulateDismissButtonClicked() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(GetDismissButton()).NotifyClick(event);
  }

  void SimulateSessionPlaying() { SimulateSessionInfo(true); }

  void SimulateSessionPaused() { SimulateSessionInfo(false); }

  void SimulateMetadataChanged() {
    media_session::MediaMetadata metadata;
    metadata.source_title = base::ASCIIToUTF16("source_title2");
    metadata.title = base::ASCIIToUTF16("title2");
    metadata.artist = base::ASCIIToUTF16("artist2");
    GetView()->UpdateWithMediaMetadata(metadata);
  }

  void SimulateAllActionsEnabled() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kStop);

    NotifyUpdatedActions();
  }

  void SimulateOnlyPlayPauseEnabled() {
    actions_.clear();
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    NotifyUpdatedActions();
  }

  void SimulateHasArtwork() {
    SkBitmap image;
    image.allocN32Pixels(10, 10);
    image.eraseColor(SK_ColorMAGENTA);
    GetView()->UpdateWithMediaArtwork(
        gfx::ImageSkia::CreateFrom1xBitmap(image));
  }

  void SimulateHasNoArtwork() {
    GetView()->UpdateWithMediaArtwork(gfx::ImageSkia());
  }

  MockMediaNotificationContainerObserver& observer() { return *observer_; }

  MediaNotificationContainerImplView* notification_container() {
    return notification_container_;
  }

 private:
  void SimulateSessionInfo(bool playing) {
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state =
        playing ? MediaPlaybackState::kPlaying : MediaPlaybackState::kPaused;
    session_info->is_controllable = true;

    GetView()->UpdateWithMediaSessionInfo(std::move(session_info));
  }

  void SimulateMediaSessionData() {
    SimulateSessionInfo(true);

    media_session::MediaMetadata metadata;
    metadata.source_title = base::ASCIIToUTF16("source_title");
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    GetView()->UpdateWithMediaMetadata(metadata);

    SimulateOnlyPlayPauseEnabled();
  }

  void NotifyUpdatedActions() { GetView()->UpdateWithMediaActions(actions_); }

  media_message_center::MediaNotificationView* GetView() {
    return notification_container()->view_for_testing();
  }

  views::ImageButton* GetDismissButton() {
    return notification_container()->GetDismissButtonForTesting();
  }

  views::Widget widget_;
  MediaNotificationContainerImplView* notification_container_ = nullptr;
  std::unique_ptr<MockMediaNotificationContainerObserver> observer_;

  // Set of actions currently enabled.
  std::set<MediaSessionAction> actions_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImplViewTest);
};

TEST_F(MediaNotificationContainerImplViewTest, SwipeToDismiss) {
  EXPECT_CALL(observer(), OnContainerDismissed(kTestNotificationId));
  SimulateNotificationSwipedToDismiss();
}

TEST_F(MediaNotificationContainerImplViewTest, ClickToDismiss) {
  // We don't show the dismiss button when playing.
  SimulateSessionPlaying();
  EXPECT_FALSE(IsDismissButtonVisible());

  // We only show it when paused.
  SimulateSessionPaused();
  EXPECT_TRUE(IsDismissButtonVisible());

  // Clicking it should inform observers that we've been dismissed.
  EXPECT_CALL(observer(), OnContainerDismissed(kTestNotificationId));
  SimulateDismissButtonClicked();
}

TEST_F(MediaNotificationContainerImplViewTest, ForceExpandedState) {
  bool notification_expanded = false;
  EXPECT_CALL(observer(), OnContainerExpanded(_))
      .WillRepeatedly([&notification_expanded](bool expanded) {
        notification_expanded = expanded;
      });

  // When we have many actions enabled, we should be forced into the expanded
  // state.
  SimulateAllActionsEnabled();
  EXPECT_TRUE(notification_expanded);

  // When we don't have many actions enabled, we should be forced out of the
  // expanded state.
  SimulateOnlyPlayPauseEnabled();
  EXPECT_FALSE(notification_expanded);

  // We will also be forced into the expanded state when artwork is present.
  SimulateHasArtwork();
  EXPECT_TRUE(notification_expanded);

  // Once the artwork is gone, we should be forced back out of the expanded
  // state.
  SimulateHasNoArtwork();
  EXPECT_FALSE(notification_expanded);
}

TEST_F(MediaNotificationContainerImplViewTest, SendsMetadataUpdates) {
  EXPECT_CALL(observer(), OnContainerMetadataChanged());
  SimulateMetadataChanged();
}

TEST_F(MediaNotificationContainerImplViewTest, SendsDestroyedUpdates) {
  auto container = std::make_unique<MediaNotificationContainerImplView>(
      kOtherTestNotificationId, nullptr);
  MockMediaNotificationContainerObserver observer;
  container->AddObserver(&observer);

  // When the container is destroyed, it should notify the observer.
  EXPECT_CALL(observer, OnContainerDestroyed(kOtherTestNotificationId));
  container.reset();
  testing::Mock::VerifyAndClearExpectations(&observer);
}
