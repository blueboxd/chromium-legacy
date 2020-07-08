// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_base.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

class TestAmbientURLLoaderImpl : public AmbientURLLoader {
 public:
  TestAmbientURLLoaderImpl() = default;
  ~TestAmbientURLLoaderImpl() override = default;

  // AmbientURLLoader:
  void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) override {
    auto data = std::make_unique<std::string>();
    *data = "test";
    // Pretend to respond asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(data)));
  }
};

class TestAmbientImageDecoderImpl : public AmbientImageDecoder {
 public:
  TestAmbientImageDecoderImpl() = default;
  ~TestAmbientImageDecoderImpl() override = default;

  // AmbientImageDecoder:
  void Decode(
      const std::vector<uint8_t>& encoded_bytes,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    // Pretend to respond asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), gfx::test::CreateImageSkia(
                                                /*width=*/10, /*height=*/10)));
  }
};

AmbientAshTestBase::AmbientAshTestBase()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

AmbientAshTestBase::~AmbientAshTestBase() = default;

void AmbientAshTestBase::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kAmbientModeFeature);
  image_downloader_ = std::make_unique<TestImageDownloader>();
  ambient_client_ = std::make_unique<TestAmbientClient>(&wake_lock_provider_);
  chromeos::PowerManagerClient::InitializeFake();

  AshTestBase::SetUp();

  // Need to reset first and then assign the TestPhotoClient because can only
  // have one instance of AmbientBackendController.
  ambient_controller()->set_backend_controller_for_testing(nullptr);
  ambient_controller()->set_backend_controller_for_testing(
      std::make_unique<FakeAmbientBackendControllerImpl>());
  photo_controller()->set_url_loader_for_testing(
      std::make_unique<TestAmbientURLLoaderImpl>());
  photo_controller()->set_image_decoder_for_testing(
      std::make_unique<TestAmbientImageDecoderImpl>());
}

void AmbientAshTestBase::TearDown() {
  ambient_client_.reset();
  image_downloader_.reset();

  AshTestBase::TearDown();
}

void AmbientAshTestBase::ShowAmbientScreen() {
  // The widget will be destroyed in |AshTestBase::TearDown()|.
  ambient_controller()->ShowUi(AmbientUiMode::kLockScreenUi);
  // Flush the message loop to finish all async calls.
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::HideAmbientScreen() {
  ambient_controller()->HideLockScreenUi();
}

void AmbientAshTestBase::CloseAmbientScreen() {
  ambient_controller()->ambient_ui_model()->SetUiVisibility(
      AmbientUiVisibility::kClosed);
}

void AmbientAshTestBase::LockScreen() {
  GetSessionControllerClient()->LockScreen();
}

void AmbientAshTestBase::UnlockScreen() {
  GetSessionControllerClient()->UnlockScreen();
}

void AmbientAshTestBase::SimulateSystemSuspendAndWait(
    power_manager::SuspendImminent::Reason reason) {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(reason);
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::SimulateSystemResumeAndWait() {
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  base::RunLoop().RunUntilIdle();
}

const gfx::ImageSkia& AmbientAshTestBase::GetImageInPhotoView() {
  return container_view()
      ->photo_view_for_testing()
      ->GetCurrentImagesForTesting();
}

int AmbientAshTestBase::GetNumOfActiveWakeLocks(
    device::mojom::WakeLockType type) {
  base::RunLoop run_loop;
  int result_count = 0;
  wake_lock_provider_.GetActiveWakeLocksForTests(
      type, base::BindOnce(
                [](base::RunLoop* run_loop, int* result_count, int32_t count) {
                  *result_count = count;
                  run_loop->Quit();
                },
                &run_loop, &result_count));
  run_loop.Run();
  return result_count;
}

void AmbientAshTestBase::IssueAccessToken(const std::string& token,
                                          bool with_error) {
  ambient_client_->IssueAccessToken(token, with_error);
}

bool AmbientAshTestBase::IsAccessTokenRequestPending() const {
  return ambient_client_->IsAccessTokenRequestPending();
}

AmbientController* AmbientAshTestBase::ambient_controller() {
  return Shell::Get()->ambient_controller();
}

AmbientPhotoController* AmbientAshTestBase::photo_controller() {
  return ambient_controller()->get_ambient_photo_controller_for_testing();
}

AmbientContainerView* AmbientAshTestBase::container_view() {
  return ambient_controller()->get_container_view_for_testing();
}

}  // namespace ash
