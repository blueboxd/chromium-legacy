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

namespace ash {

// The base class to test the ambient mode in Ash.
class AmbientAshTestBase : public AshTestBase {
 public:
  AmbientAshTestBase();
  ~AmbientAshTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  AmbientController* ambient_controller() const;

  AmbientPhotoController* photo_controller();

  void LockScreen();
  void UnlockScreen();

  // Toggle ambient mode.
  void Toggle();

  // Simulate to issue an |access_token|.
  // If |with_error| is true, will return an empty access token.
  void IssueAccessToken(const std::string& access_token, bool with_error);

  bool IsAccessTokenRequestPending() const;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestImageDownloader> image_downloader_;
  std::unique_ptr<TestAmbientClient> ambient_client_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
