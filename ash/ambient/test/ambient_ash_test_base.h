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
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
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

  // Closes ambient screen. Can only be called after |ShowAmbientScreen| has
  // been called.
  void CloseAmbientScreen();

  // Simulates user locks/unlocks screen which will result in ambient widget
  // shown/closed.
  void LockScreen();
  void UnlockScreen();

  // Simulates the system starting to suspend with Reason |reason|.
  // Wait until the event has been processed.
  void SimulateSystemSuspendAndWait(
      power_manager::SuspendImminent::Reason reason);

  // Simulates the system starting to resume.
  // Wait until the event has been processed.
  void SimulateSystemResumeAndWait();

  const gfx::ImageSkia& GetImageInPhotoView();

  // Returns the number of active wake locks of type |type|.
  int GetNumOfActiveWakeLocks(device::mojom::WakeLockType type);

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

  device::TestWakeLockProvider wake_lock_provider_;
  std::unique_ptr<TestAmbientClient> ambient_client_;
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
