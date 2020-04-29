// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"

namespace ash {

using AmbientPhotoControllerTest = AmbientAshTestBase;

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
