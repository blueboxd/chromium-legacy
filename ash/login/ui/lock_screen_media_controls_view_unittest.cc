// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"

#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/media_controls_header_view.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

const int kAppIconSize = 20;
constexpr int kArtworkViewWidth = 270;
constexpr int kArtworkViewHeight = 200;
const base::string16 kTestAppName = base::ASCIIToUTF16("Test app");

}  // namespace

class LockScreenMediaControlsViewTest : public LoginTestBase {
 public:
  LockScreenMediaControlsViewTest() = default;
  ~LockScreenMediaControlsViewTest() override = default;

  void SetUp() override {
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
  }

  MediaControlsHeaderView* header_row() const {
    return media_controls_view_->header_row_;
  }

  views::ImageView* artwork_view() const {
    return media_controls_view_->session_artwork_;
  }

  const gfx::ImageSkia& GetAppIcon() const {
    return header_row()->app_icon_for_testing();
  }

  const base::string16& GetAppName() const {
    return header_row()->app_name_for_testing();
  }

  LockScreenMediaControlsView* media_controls_view_ = nullptr;

 private:
  LockContentsView* lock_contents_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockScreenMediaControlsViewTest);
};

TEST_F(LockScreenMediaControlsViewTest, UpdateAppIcon) {
  gfx::ImageSkia default_icon = gfx::CreateVectorIcon(
      message_center::kProductIcon, kAppIconSize, gfx::kChromeIconGrey);

  // Verify that the icon is initialized to the default.
  EXPECT_TRUE(GetAppIcon().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, GetAppIcon().width());
  EXPECT_EQ(kAppIconSize, GetAppIcon().height());

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());

  // Verify that the default icon is used if no icon is provided.
  EXPECT_TRUE(GetAppIcon().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, GetAppIcon().width());
  EXPECT_EQ(kAppIconSize, GetAppIcon().height());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kAppIconSize, kAppIconSize);
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, bitmap);

  // Verify that the provided icon is used.
  EXPECT_FALSE(GetAppIcon().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, GetAppIcon().width());
  EXPECT_EQ(kAppIconSize, GetAppIcon().height());
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