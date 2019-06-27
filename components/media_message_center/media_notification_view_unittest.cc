// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/unguessable_token.h"
#include "components/media_message_center/media_notification_background.h"
#include "components/media_message_center/media_notification_constants.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_notification_item.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;
using testing::_;
using testing::Expectation;
using testing::Invoke;

namespace {

// The icons size is 24 and INSETS_VECTOR_IMAGE_BUTTON will add padding around
// the image.
const int kMediaButtonIconSize = 24;

// The title artist row should always have the same height.
const int kMediaTitleArtistRowExpectedHeight = 48;

const char kTestDefaultAppName[] = "default app name";
const char kTestAppName[] = "app name";

const gfx::Size kWidgetSize(500, 500);
const gfx::Size kViewSize(400, 400);

// Checks if the view class name is used by a media button.
bool IsMediaButtonType(const char* class_name) {
  return class_name == views::ImageButton::kViewClassName ||
         class_name == views::ToggleImageButton::kViewClassName;
}

class MockMediaNotificationController : public MediaNotificationController {
 public:
  MockMediaNotificationController() = default;
  ~MockMediaNotificationController() = default;

  // MediaNotificationController implementation.
  MOCK_METHOD1(ShowNotification, void(const std::string& id));
  MOCK_METHOD1(HideNotification, void(const std::string& id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationController);
};

class MockMediaNotificationContainer : public MediaNotificationContainer {
 public:
  MockMediaNotificationContainer() = default;
  ~MockMediaNotificationContainer() = default;

  // MediaNotificationContainer implementation.
  MOCK_METHOD1(OnExpanded, void(bool expanded));

  MediaNotificationView* view() const { return view_.get(); }
  void SetView(std::unique_ptr<MediaNotificationView> view) {
    view_ = std::move(view);
  }

 private:
  std::unique_ptr<MediaNotificationView> view_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationContainer);
};

}  // namespace

class MediaNotificationViewTest : public views::ViewsTestBase {
 public:
  MediaNotificationViewTest() = default;
  ~MediaNotificationViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    request_id_ = base::UnguessableToken::Create();

    // Create a new MediaNotificationView whenever the MediaNotificationItem
    // says to show the notification.
    EXPECT_CALL(controller_, ShowNotification(request_id_.ToString()))
        .WillRepeatedly(
            InvokeWithoutArgs(this, &MediaNotificationViewTest::CreateView));

    // Create a widget to show on the screen for testing screen coordinates and
    // focus.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(kWidgetSize);
    widget_->Init(params);
    widget_->Show();

    CreateViewFromMediaSessionInfo(
        media_session::mojom::MediaSessionInfo::New());
  }

  void CreateViewFromMediaSessionInfo(
      media_session::mojom::MediaSessionInfoPtr session_info) {
    session_info->is_controllable = true;
    media_session::mojom::MediaControllerPtr controller;
    item_ = std::make_unique<MediaNotificationItem>(
        &controller_, request_id_.ToString(), std::string(),
        std::move(controller), std::move(session_info));

    // Update the metadata.
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    item_->MediaSessionMetadataChanged(metadata);

    // Inject the test media controller into the item.
    media_controller_ = std::make_unique<TestMediaController>();
    item_->SetMediaControllerForTesting(
        media_controller_->CreateMediaControllerPtr());
  }

  void TearDown() override {
    widget_.reset();

    actions_.clear();

    views::ViewsTestBase::TearDown();
  }

  void EnableAllActions() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kStop);

    NotifyUpdatedActions();
  }

  void EnableAction(MediaSessionAction action) {
    actions_.insert(action);
    NotifyUpdatedActions();
  }

  void DisableAction(MediaSessionAction action) {
    actions_.erase(action);
    NotifyUpdatedActions();
  }

  MockMediaNotificationContainer& container() { return container_; }

  MediaNotificationView* view() const { return container_.view(); }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  message_center::NotificationHeaderView* header_row() const {
    return view()->header_row_;
  }

  const base::string16& accessible_name() const {
    return view()->accessible_name_;
  }

  views::View* button_row() const { return view()->button_row_; }

  views::View* title_artist_row() const { return view()->title_artist_row_; }

  views::Label* title_label() const { return view()->title_label_; }

  views::Label* artist_label() const { return view()->artist_label_; }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    const auto& children = button_row()->children();
    const auto i = std::find_if(
        children.begin(), children.end(), [action](const views::View* v) {
          return views::Button::AsButton(v)->tag() == static_cast<int>(action);
        });
    return (i == children.end()) ? nullptr : views::Button::AsButton(*i);
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    return GetButtonForAction(action)->GetVisible();
  }

  MediaNotificationItem* GetItem() const { return item_.get(); }

  const gfx::ImageSkia& GetArtworkImage() const {
    return view()->GetMediaNotificationBackground()->artwork_;
  }

  const gfx::ImageSkia& GetAppIcon() const {
    return header_row()->app_icon_for_testing();
  }

  bool expand_button_enabled() const {
    return header_row()->expand_button()->GetVisible();
  }

  bool IsActuallyExpanded() const { return view()->IsActuallyExpanded(); }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    view()->ButtonPressed(
        button, ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0));
  }

  void SimulateHeaderClick() {
    view()->ButtonPressed(
        header_row(),
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  void SimulateTab() {
    ui::KeyEvent pressed_tab(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
    view()->GetFocusManager()->OnKeyEvent(pressed_tab);
  }

  void ExpectHistogramActionRecorded(MediaSessionAction action) {
    histogram_tester_.ExpectUniqueSample(
        MediaNotificationItem::kUserActionHistogramName,
        static_cast<base::HistogramBase::Sample>(action), 1);
  }

  void ExpectHistogramArtworkRecorded(bool present, int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationView::kArtworkHistogramName,
        static_cast<base::HistogramBase::Sample>(present), count);
  }

  void ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata metadata,
                                       int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationView::kMetadataHistogramName,
        static_cast<base::HistogramBase::Sample>(metadata), count);
  }

 private:
  void NotifyUpdatedActions() {
    item_->MediaSessionActionsChanged(
        std::vector<MediaSessionAction>(actions_.begin(), actions_.end()));
  }

  void CreateView() {
    // Create a MediaNotificationView.
    auto view = std::make_unique<MediaNotificationView>(
        &container_, item_->GetWeakPtr(),
        nullptr /* header_row_controls_view */,
        base::ASCIIToUTF16(kTestDefaultAppName));
    view->SetSize(kViewSize);
    view->set_owned_by_client();

    // Display it in |widget_|.
    widget_->SetContentsView(view.get());

    // Associate it with |container_|.
    container_.SetView(std::move(view));
  }

  base::UnguessableToken request_id_;

  base::HistogramTester histogram_tester_;

  std::set<MediaSessionAction> actions_;

  std::unique_ptr<TestMediaController> media_controller_;
  MockMediaNotificationContainer container_;
  MockMediaNotificationController controller_;
  std::unique_ptr<MediaNotificationItem> item_;
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationViewTest);
};

TEST_F(MediaNotificationViewTest, ButtonsSanityCheck) {
  view()->SetExpanded(true);

  EnableAllActions();

  EXPECT_TRUE(button_row()->GetVisible());
  EXPECT_GT(button_row()->width(), 0);
  EXPECT_GT(button_row()->height(), 0);

  EXPECT_EQ(5u, button_row()->children().size());

  for (auto* child : button_row()->children()) {
    ASSERT_TRUE(IsMediaButtonType(child->GetClassName()));

    EXPECT_TRUE(child->GetVisible());
    EXPECT_LT(kMediaButtonIconSize, child->width());
    EXPECT_LT(kMediaButtonIconSize, child->height());
    EXPECT_FALSE(views::Button::AsButton(child)->GetAccessibleName().empty());
  }

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));

  // |kPause| cannot be present if |kPlay| is.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
}

TEST_F(MediaNotificationViewTest, ButtonsFocusCheck) {
  // Expand and enable all actions to show all buttons.
  view()->SetExpanded(true);
  EnableAllActions();

  views::FocusManager* focus_manager = view()->GetFocusManager();

  {
    // Focus the first action button.
    auto* button = GetButtonForAction(MediaSessionAction::kPreviousTrack);
    focus_manager->SetFocusedView(button);
    EXPECT_EQ(button, focus_manager->GetFocusedView());
  }

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekBackward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPlay),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekForward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kNextTrack),
            focus_manager->GetFocusedView());
}

TEST_F(MediaNotificationViewTest, PlayPauseButtonTooltipCheck) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  auto* button = GetButtonForAction(MediaSessionAction::kPlay);
  base::string16 tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  base::string16 new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(MediaNotificationViewTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kNextTrack);
}

TEST_F(MediaNotificationViewTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  SimulateButtonClick(MediaSessionAction::kPlay);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPlay);
}

TEST_F(MediaNotificationViewTest, PauseButtonClick) {
  EnableAction(MediaSessionAction::kPause);

  EXPECT_EQ(0, media_controller()->suspend_count());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  SimulateButtonClick(MediaSessionAction::kPause);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->suspend_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPause);
}

TEST_F(MediaNotificationViewTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPreviousTrack);
}

TEST_F(MediaNotificationViewTest, SeekBackwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekBackward);
}

TEST_F(MediaNotificationViewTest, SeekForwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  SimulateButtonClick(MediaSessionAction::kSeekForward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekForward);
}

TEST_F(MediaNotificationViewTest, PlayToggle_FromObserver_Empty) {
  EnableAction(MediaSessionAction::kPlay);

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }

  view()->UpdateWithMediaSessionInfo(
      media_session::mojom::MediaSessionInfo::New());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }
}

TEST_F(MediaNotificationViewTest, PlayToggle_FromObserver_PlaybackState) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPause));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_TRUE(button->toggled_for_testing());
  }

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }
}

TEST_F(MediaNotificationViewTest, MetadataIsDisplayed) {
  view()->SetExpanded(true);

  EnableAllActions();

  EXPECT_TRUE(title_artist_row()->GetVisible());
  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(artist_label()->GetVisible());

  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver) {
  EnableAllActions();

  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kTitle, 1);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kArtist, 1);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kAlbum, 0);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kCount, 1);

  EXPECT_FALSE(header_row()->summary_text_for_testing()->GetVisible());

  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  metadata.album = base::ASCIIToUTF16("album");

  GetItem()->MediaSessionMetadataChanged(metadata);
  view()->SetExpanded(true);

  EXPECT_TRUE(title_artist_row()->GetVisible());
  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(artist_label()->GetVisible());
  EXPECT_TRUE(header_row()->summary_text_for_testing()->GetVisible());

  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
  EXPECT_EQ(metadata.album,
            header_row()->summary_text_for_testing()->GetText());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());

  EXPECT_EQ(base::ASCIIToUTF16("title2 - artist2 - album"), accessible_name());

  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kTitle, 2);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kArtist, 2);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kAlbum, 1);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kCount, 2);
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_AppName) {
  EXPECT_EQ(base::ASCIIToUTF16(kTestDefaultAppName),
            header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    metadata.source_title = base::ASCIIToUTF16(kTestAppName);
    GetItem()->MediaSessionMetadataChanged(metadata);
  }

  EXPECT_EQ(base::ASCIIToUTF16(kTestAppName),
            header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    GetItem()->MediaSessionMetadataChanged(metadata);
  }

  EXPECT_EQ(base::ASCIIToUTF16(kTestDefaultAppName),
            header_row()->app_name_for_testing());
}

TEST_F(MediaNotificationViewTest, Buttons_WhenCollapsed) {
  EnableAllActions();

  view()->SetExpanded(false);

  EXPECT_FALSE(IsActuallyExpanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));

  DisableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  EnableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  DisableAction(MediaSessionAction::kSeekForward);
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));

  EnableAction(MediaSessionAction::kSeekForward);
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MediaNotificationViewTest, Buttons_WhenExpanded) {
  EnableAllActions();

  view()->SetExpanded(true);

  EXPECT_TRUE(IsActuallyExpanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MediaNotificationViewTest, ClickHeader_ToggleExpand) {
  view()->SetExpanded(true);
  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());

  SimulateHeaderClick();

  EXPECT_FALSE(IsActuallyExpanded());

  SimulateHeaderClick();

  EXPECT_TRUE(IsActuallyExpanded());
}

TEST_F(MediaNotificationViewTest, ActionButtonsHiddenByDefault) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaNotificationViewTest, ActionButtonsToggleVisibility) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  DisableAction(MediaSessionAction::kNextTrack);

  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
}

TEST_F(MediaNotificationViewTest, UpdateArtworkFromItem) {
  int title_artist_width = title_artist_row()->width();
  const SkColor accent = header_row()->accent_color_for_testing();
  gfx::Size size = view()->size();

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  EXPECT_TRUE(GetArtworkImage().isNull());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  ExpectHistogramArtworkRecorded(true, 1);

  // Ensure the title artist row has a small width than before now that we
  // have artwork.
  EXPECT_GT(title_artist_width, title_artist_row()->width());

  // Ensure that the image is displayed in the background artwork and that the
  // size of the notification was not affected.
  EXPECT_FALSE(GetArtworkImage().isNull());
  EXPECT_EQ(gfx::Size(10, 10), GetArtworkImage().size());
  EXPECT_EQ(size, view()->size());
  EXPECT_NE(accent, header_row()->accent_color_for_testing());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  ExpectHistogramArtworkRecorded(false, 1);

  // Ensure the title artist row goes back to the original width now that we
  // do not have any artwork.
  EXPECT_EQ(title_artist_width, title_artist_row()->width());

  // Ensure that the background artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
  EXPECT_EQ(accent, header_row()->accent_color_for_testing());
}

TEST_F(MediaNotificationViewTest, ExpandableDefaultState) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, ExpandablePlayPauseActionCountsOnce) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kPreviousTrack);
  EnableAction(MediaSessionAction::kNextTrack);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, BecomeExpandableAndWasNotExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, BecomeExpandableButWasAlreadyExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, BecomeNotExpandableAndWasExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kPreviousTrack);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kSeekBackward);
  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest,
       BecomeNotExpandableButWasAlreadyNotExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, ActionButtonRowSizeAndAlignment) {
  EnableAction(MediaSessionAction::kPlay);

  views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
  int button_x = button->GetBoundsInScreen().x();

  // When collapsed the button row should be a fixed width.
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_EQ(124, button_row()->width());

  EnableAllActions();
  view()->SetExpanded(true);

  // When expanded the button row should be wider and the play button should
  // have shifted to the left.
  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_LT(124, button_row()->width());
  EXPECT_GT(button_x, button->GetBoundsInScreen().x());
}

TEST_F(MediaNotificationViewTest, NotifysContainerOfExpandedState) {
  // Track the expanded state given to |container_|.
  bool expanded = false;
  EXPECT_CALL(container(), OnExpanded(_))
      .WillRepeatedly(Invoke([&expanded](bool exp) { expanded = exp; }));

  // Expand the view implicitly via |EnableAllActions()|.
  view()->SetExpanded(true);
  EnableAllActions();
  EXPECT_TRUE(expanded);

  // Explicitly contract the view.
  view()->SetExpanded(false);
  EXPECT_FALSE(expanded);

  // Explicitly expand the view.
  view()->SetExpanded(true);
  EXPECT_TRUE(expanded);

  // Implicitly contract the view by removing available actions.
  DisableAction(MediaSessionAction::kPreviousTrack);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kSeekBackward);
  DisableAction(MediaSessionAction::kSeekForward);
  EXPECT_FALSE(expanded);
}

TEST_F(MediaNotificationViewTest, AccessibleNodeData) {
  ui::AXNodeData data;
  view()->GetAccessibleNodeData(&data);

  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(base::ASCIIToUTF16("title - artist"), accessible_name());
}

}  // namespace media_message_center
