// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
#define ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_

#include <memory>
#include <string>

#include "ash/ambient/ambient_controller.h"
#include "ash/public/cpp/test/test_ambient_client.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientContainerView;
class AmbientPhotoController;

// The base class to test the Ambient Mode in Ash.
class AmbientAshTestBase : public AshTestBase {
 public:
  AmbientAshTestBase();
  ~AmbientAshTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Creates ambient screen in its own widget.
  void ShowAmbientScreen();

  // Hides ambient screen. Can only be called after |ShowAmbientScreen| has been
  // called.
  void HideAmbientScreen();

  // Simulates user locks/unlocks screen which will result in ambient widget
  // shown/closed.
  void LockScreen();
  void UnlockScreen();

  const gfx::ImageSkia& GetImageInPhotoView();

  // Simulate to issue an |access_token|.
  // If |with_error| is true, will return an empty access token.
  void IssueAccessToken(const std::string& access_token, bool with_error);

  bool IsAccessTokenRequestPending() const;

  AmbientController* ambient_controller();

  AmbientPhotoController* photo_controller();

  // Returns the top-level view which contains all the ambient components.
  AmbientContainerView* container_view();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestImageDownloader> image_downloader_;
  std::unique_ptr<TestAmbientClient> ambient_client_;
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
