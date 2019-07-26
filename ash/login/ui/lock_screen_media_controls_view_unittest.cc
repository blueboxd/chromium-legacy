// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"

#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/media_controls_header_view.h"
#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/controls/image_view.h"

namespace ash {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;

namespace {

const int kAppIconSize = 20;
constexpr int kArtworkViewWidth = 270;
constexpr int kArtworkViewHeight = 200;

const base::string16 kTestAppName = base::ASCIIToUTF16("Test app");

MediaSessionAction kActionButtonOrder[] = {
    MediaSessionAction::kPreviousTrack, MediaSessionAction::kSeekBackward,
    MediaSessionAction::kPause, MediaSessionAction::kSeekForward,
    MediaSessionAction::kNextTrack};

// Checks if the view class name is used by a media button.
bool IsMediaButtonType(const char* class_name) {
  return class_name == views::ImageButton::kViewClassName ||
         class_name == views::ToggleImageButton::kViewClassName;
}

}  // namespace

class LockScreenMediaControlsViewTest : public LoginTestBase {
 public:
  LockScreenMediaControlsViewTest() = default;
  ~LockScreenMediaControlsViewTest() override = default;

  void SetUp() override {
    // Enable media controls.
    feature_list.InitWithFeatures(
        {features::kLockScreenMediaKeys, features::kLockScreenMediaControls},
        {});

    LoginTestBase::SetUp();

    lock_contents_view_ = new LockContentsView(
        mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
        DataDispatcher(),
        std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
    LockContentsView::TestApi lock_contents(lock_contents_view_);

    std::unique_ptr<views::Widget> widget =
        CreateWidgetWithContent(lock_contents_view_);
    SetWidget(std::move(widget));

    SetUserCount(1);

    media_controls_view_ = lock_contents.media_controls_view();

    // Inject the test media controller into the media controls view.
    media_controller_ = std::make_unique<TestMediaController>();
    media_controls_view_->set_media_controller_for_testing(
        media_controller_->CreateMediaControllerPtr());

    SimulateMediaSessionChanged(
        media_session::mojom::MediaPlaybackState::kPlaying);
  }

  void TearDown() override {
    actions_.clear();

    LoginTestBase::TearDown();
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

  void SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState playback_state) {
    // Simulate media session change.
    media_controls_view_->MediaSessionChanged(base::UnguessableToken::Create());

    // Create media session information.
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state = playback_state;

    // Simulate media session information change.
    media_controls_view_->MediaSessionInfoChanged(session_info.Clone());
  }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    // Send event to click media playback action button.
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

  void SimulateTab() {
    ui::KeyEvent pressed_tab(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
    media_controls_view_->GetFocusManager()->OnKeyEvent(pressed_tab);
  }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    const auto& children = button_row()->children();
    const auto it = std::find_if(
        children.begin(), children.end(), [action](const views::View* v) {
          return views::Button::AsButton(v)->tag() == static_cast<int>(action);
        });

    if (it == children.end())
      return nullptr;

    return views::Button::AsButton(*it);
  }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  MediaControlsHeaderView* header_row() const {
    return media_controls_view_->header_row_;
  }

  NonAccessibleView* button_row() const {
    return media_controls_view_->button_row_;
  }

  views::ImageView* artwork_view() const {
    return media_controls_view_->session_artwork_;
  }

  views::ImageButton* close_button() const {
    return media_controls_view_->close_button_;
  }

  const views::ImageView* icon_view() const {
    return header_row()->app_icon_for_testing();
  }

  const base::string16& GetAppName() const {
    return header_row()->app_name_for_testing();
  }

  LockScreenMediaControlsView* media_controls_view_ = nullptr;

 private:
  void NotifyUpdatedActions() {
    media_controls_view_->MediaSessionActionsChanged(
        std::vector<MediaSessionAction>(actions_.begin(), actions_.end()));
  }

  base::test::ScopedFeatureList feature_list;

  LockContentsView* lock_contents_view_ = nullptr;
  std::unique_ptr<TestMediaController> media_controller_;
  std::set<MediaSessionAction> actions_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenMediaControlsViewTest);
};

TEST_F(LockScreenMediaControlsViewTest, KeepMediaSessionDataBetweenSessions) {
  // Simulate media session stopping.
  media_controls_view_->MediaSessionChanged(base::nullopt);

  // Simulate new media session starting within the timer delay.
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Set icon for new media session.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kAppIconSize, kAppIconSize);
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, bitmap);

  // Default icon.
  gfx::ImageSkia default_icon = gfx::CreateVectorIcon(
      message_center::kProductIcon, kAppIconSize, gfx::kChromeIconGrey);

  // Verify that the default icon is not drawn.
  EXPECT_FALSE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));

  // Set artwork for new media session.
  SkBitmap artwork;
  artwork.allocN32Pixels(kArtworkViewWidth, kArtworkViewHeight);
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  // Verify that the default artwork is not drawn.
  EXPECT_FALSE(artwork_view()->GetImage().isNull());

  // Set app name for new media session.
  media_session::MediaMetadata metadata;
  metadata.source_title = kTestAppName;
  media_controls_view_->MediaSessionMetadataChanged(metadata);

  // Verify that the default name is not used.
  EXPECT_EQ(kTestAppName, GetAppName());
}

TEST_F(LockScreenMediaControlsViewTest, ButtonsSanityCheck) {
  EnableAllActions();

  EXPECT_TRUE(button_row()->GetVisible());
  EXPECT_EQ(5u, button_row()->children().size());

  for (int i = 0; i < 5; /* size of |button_row| */ i++) {
    auto* child = button_row()->children()[i];

    ASSERT_TRUE(IsMediaButtonType(child->GetClassName()));

    ASSERT_EQ(
        static_cast<MediaSessionAction>(views::Button::AsButton(child)->tag()),
        kActionButtonOrder[i]);

    EXPECT_TRUE(child->GetVisible());
    EXPECT_FALSE(views::Button::AsButton(child)->GetAccessibleName().empty());
  }

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));

  // |kPlay| cannot be present if |kPause| is.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
}

TEST_F(LockScreenMediaControlsViewTest, ButtonsFocusCheck) {
  EnableAllActions();

  views::FocusManager* focus_manager = media_controls_view_->GetFocusManager();

  {
    // Focus the first action button - the close button.
    focus_manager->SetFocusedView(close_button());
    EXPECT_EQ(close_button(), focus_manager->GetFocusedView());
  }

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPreviousTrack),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekBackward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPause),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekForward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kNextTrack),
            focus_manager->GetFocusedView());
}

TEST_F(LockScreenMediaControlsViewTest, PlayPauseButtonTooltipCheck) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  auto* button = GetButtonForAction(MediaSessionAction::kPause);
  base::string16 tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  media_controls_view_->MediaSessionInfoChanged(session_info.Clone());

  base::string16 new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(LockScreenMediaControlsViewTest, CloseButtonVisibility) {
  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_FALSE(close_button()->IsDrawn());

  // Move the mouse inside |media_controls_view_|.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view_->GetBoundsInScreen().CenterPoint());

  // Verify that the close button is shown.
  EXPECT_TRUE(close_button()->IsDrawn());

  // Move the mouse outside |media_controls_view_|.
  generator->MoveMouseBy(500, 500);

  // Verify that the close button is hidden.
  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_FALSE(close_button()->IsDrawn());
}

TEST_F(LockScreenMediaControlsViewTest, CloseButtonClick) {
  EXPECT_TRUE(media_controls_view_->IsDrawn());

  // Move the mouse inside |media_controls_view_|.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view_->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_EQ(0, media_controller()->stop_count());

  // Send event to click the close button.
  generator->MoveMouseTo(close_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Verify that the media was stopped.
  media_controls_view_->FlushForTesting();
  EXPECT_EQ(1, media_controller()->stop_count());

  // Verify that the controls were hidden.
  EXPECT_FALSE(media_controls_view_->IsDrawn());
}

TEST_F(LockScreenMediaControlsViewTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
}

TEST_F(LockScreenMediaControlsViewTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  media_controls_view_->MediaSessionInfoChanged(session_info.Clone());

  SimulateButtonClick(MediaSessionAction::kPlay);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());
}

TEST_F(LockScreenMediaControlsViewTest, PauseButtonClick) {
  EnableAction(MediaSessionAction::kPause);

  EXPECT_EQ(0, media_controller()->suspend_count());

  SimulateButtonClick(MediaSessionAction::kPause);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->suspend_count());
}

TEST_F(LockScreenMediaControlsViewTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
}

TEST_F(LockScreenMediaControlsViewTest, SeekBackwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());
}

TEST_F(LockScreenMediaControlsViewTest, SeekForwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  SimulateButtonClick(MediaSessionAction::kSeekForward);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateAppIcon) {
  gfx::ImageSkia default_icon = gfx::CreateVectorIcon(
      message_center::kProductIcon, kAppIconSize, gfx::kChromeIconGrey);

  // Verify that the icon is initialized to the default.
  EXPECT_TRUE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().width());
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().height());

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());

  // Verify that the default icon is used if no icon is provided.
  EXPECT_TRUE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().width());
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().height());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kAppIconSize, kAppIconSize);
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, bitmap);

  // Verify that the provided icon is used.
  EXPECT_FALSE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().width());
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().height());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateAppName) {
  // Verify that the app name is initialized to the default.
  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      GetAppName());

  media_session::MediaMetadata metadata;
  media_controls_view_->MediaSessionMetadataChanged(metadata);

  // Verify that default name is used if no name is provided.
  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      GetAppName());

  metadata.source_title = kTestAppName;
  media_controls_view_->MediaSessionMetadataChanged(metadata);

  // Verify that the provided app name is used.
  EXPECT_EQ(kTestAppName, GetAppName());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateImagesConvertColors) {
  SkBitmap artwork;
  SkImageInfo artwork_info =
      SkImageInfo::Make(200, 200, kAlpha_8_SkColorType, kOpaque_SkAlphaType);
  artwork.allocPixels(artwork_info);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  // Verify the artwork color was converted.
  EXPECT_EQ(artwork_view()->GetImage().bitmap()->colorType(), kN32_SkColorType);

  // Verify the artwork is visible.
  EXPECT_TRUE(artwork_view()->GetVisible());

  SkBitmap icon;
  SkImageInfo icon_info =
      SkImageInfo::Make(20, 20, kAlpha_8_SkColorType, kOpaque_SkAlphaType);
  artwork.allocPixels(icon_info);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, icon);

  // Verify the icon color was converted.
  EXPECT_EQ(icon_view()->GetImage().bitmap()->colorType(), kN32_SkColorType);

  // Verify the icon is visible.
  EXPECT_TRUE(icon_view()->GetVisible());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateArtwork) {
  // Verify that the artwork is initially empty.
  EXPECT_TRUE(artwork_view()->GetImage().isNull());

  // Create artwork that must be scaled down to fit the view.
  SkBitmap artwork;
  artwork.allocN32Pixels(540, 300);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();

  // Verify that the provided artwork is correctly scaled down.
  EXPECT_EQ(kArtworkViewWidth, artwork_bounds.width());
  EXPECT_EQ(150, artwork_bounds.height());

  // Create artwork that must be scaled up to fit the view.
  artwork.allocN32Pixels(200, 190);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  artwork_bounds = artwork_view()->GetImageBounds();

  // Verify that the provided artwork is correctly scaled up.
  EXPECT_EQ(210, artwork_bounds.width());
  EXPECT_EQ(kArtworkViewHeight, artwork_bounds.height());

  // Create artwork that already fits the view size.
  artwork.allocN32Pixels(250, kArtworkViewHeight);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  artwork_bounds = artwork_view()->GetImageBounds();

  // Verify that the provided artwork size doesn't change.
  EXPECT_EQ(250, artwork_bounds.width());
  EXPECT_EQ(kArtworkViewHeight, artwork_bounds.height());
}

TEST_F(LockScreenMediaControlsViewTest, AccessibleNodeData) {
  ui::AXNodeData data;
  media_controls_view_->GetAccessibleNodeData(&data);

  // Verify that the accessible name is initially empty.
  EXPECT_FALSE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");
  media_controls_view_->MediaSessionMetadataChanged(metadata);
  media_controls_view_->GetAccessibleNodeData(&data);

  // Verify that the accessible name updates with the metadata.
  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(base::ASCIIToUTF16("title - artist"),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

}  // namespace ash