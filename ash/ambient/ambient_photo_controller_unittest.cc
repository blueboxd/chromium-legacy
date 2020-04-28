// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <memory>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class AmbientPhotoControllerTest : public AshTestBase {
 public:
  AmbientPhotoControllerTest() = default;
  ~AmbientPhotoControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kAmbientModeFeature);
    AshTestBase::SetUp();

    // Will extract this into AmbientAshTestBase.
    // Need to reset first and then assign the TestPhotoClient because can only
    // have one instance of AmbientBackendController.
    Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
        nullptr);
    Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
        std::make_unique<FakeAmbientBackendControllerImpl>());
  }

  AmbientPhotoController* photo_controller() {
    return Shell::Get()
        ->ambient_controller()
        ->get_ambient_photo_controller_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AmbientPhotoControllerTest, ShouldGetScreenUpdateSuccessfully) {
  base::OnceClosure photo_closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::OnceClosure weather_inf_closure =
      base::MakeExpectedRunClosure(FROM_HERE);

  base::RunLoop run_loop;
  auto on_done = base::BarrierClosure(
      2, base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  photo_controller()->GetNextScreenUpdateInfo(
      base::BindLambdaForTesting([&](const gfx::ImageSkia&) {
        std::move(photo_closure).Run();
        on_done.Run();
      }),

      base::BindLambdaForTesting(
          [&](base::Optional<float>, const gfx::ImageSkia&) {
            std::move(weather_inf_closure).Run();
            on_done.Run();
          }));

  run_loop.Run();
}

}  // namespace ash
