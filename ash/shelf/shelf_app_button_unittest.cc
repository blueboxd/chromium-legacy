// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_app_button.h"

#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"

namespace ash {

class ShelfAppButtonTest : public AshTestBase {
 public:
  ShelfAppButtonTest() {
    scoped_feature_list_.InitWithFeatures({features::kNotificationIndicator},
                                          {});
  }
  ~ShelfAppButtonTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
  }

  void TearDown() override {
    test_api_.reset();
    AshTestBase::TearDown();
  }

  ShelfViewTestAPI* test_api() { return test_api_.get(); }

  void SetImageForButton(int button_num, gfx::ImageSkia image) {
    test_api()->GetButton(button_num)->SetImage(image);
  }

  SkColor GetNotificationColorForButton(int button_num) {
    return test_api()
        ->GetButton(button_num)
        ->GetNotificationIndicatorColorForTest();
  }

 private:
  std::unique_ptr<ShelfViewTestAPI> test_api_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the notification indicator has a color which is calculated
// correctly when an image is set for the ShelfAppButton.
TEST_F(ShelfAppButtonTest, NotificatonBadgeColor) {
  ShelfTestUtil::AddAppShortcut("app_id", TYPE_PINNED_APP);

  const int width = 64;
  const int height = 64;

  SkBitmap all_black_icon;
  all_black_icon.allocN32Pixels(width, height);
  all_black_icon.eraseColor(SK_ColorBLACK);

  SetImageForButton(0, gfx::ImageSkia::CreateFrom1xBitmap(all_black_icon));

  // For an all black icon, a white notification badge is expected, since there
  // is no other light vibrant color to get from the icon.
  EXPECT_EQ(SK_ColorWHITE, GetNotificationColorForButton(0));

  // Create an icon that is half kGoogleRed300 and half kGoogleRed600.
  SkBitmap red_icon;
  red_icon.allocN32Pixels(width, height);
  red_icon.eraseColor(gfx::kGoogleRed300);
  red_icon.erase(gfx::kGoogleRed600, {0, 0, width, height / 2});

  SetImageForButton(0, gfx::ImageSkia::CreateFrom1xBitmap(red_icon));

  // For the red icon, the notification badge should calculate and use the
  // kGoogleRed300 color as the light vibrant color taken from the icon.
  EXPECT_EQ(gfx::kGoogleRed300, GetNotificationColorForButton(0));
}

}  // namespace ash
