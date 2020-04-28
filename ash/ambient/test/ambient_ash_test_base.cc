// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_base.h"

#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

AmbientAshTestBase::AmbientAshTestBase()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

AmbientAshTestBase::~AmbientAshTestBase() = default;

void AmbientAshTestBase::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kAmbientModeFeature);
  assistant_image_downloader_ =
      std::make_unique<TestAssistantImageDownloader>();
  ambient_client_ = std::make_unique<TestAmbientClient>();

  AshTestBase::SetUp();

  // Need to reset first and then assign the TestPhotoClient because can only
  // have one instance of AmbientBackendController.
  Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
      nullptr);
  Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
      std::make_unique<FakeAmbientBackendControllerImpl>());
}

void AmbientAshTestBase::TearDown() {
  AshTestBase::TearDown();

  ambient_client_.reset();
  assistant_image_downloader_.reset();
}

AmbientController* AmbientAshTestBase::ambient_controller() {
  return Shell::Get()->ambient_controller();
}

AmbientPhotoController* AmbientAshTestBase::photo_controller() {
  return ambient_controller()->get_ambient_photo_controller_for_testing();
}

void AmbientAshTestBase::LockScreen() {
  GetSessionControllerClient()->LockScreen();
}

void AmbientAshTestBase::Toggle() {
  ambient_controller()->Toggle();
}

}  // namespace ash
