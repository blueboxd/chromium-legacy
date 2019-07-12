// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_winrt.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

// Tests that PlatformSensorProviderWinrt can successfully be instantiated
// and passes the correct result to the CreateSensor callback.
TEST(PlatformSensorProviderTestWinrt, SensorCreationReturnCheck) {
  base::test::ScopedTaskEnvironment scoped_task_environment;

  auto provider = std::make_unique<PlatformSensorProviderWinrt>();

  // CreateSensor is async so create a RunLoop to wait for completion.
  base::RunLoop run_loop;

  base::Callback<void(scoped_refptr<PlatformSensor> sensor)>
      CreateSensorCallback =
          base::BindLambdaForTesting([&](scoped_refptr<PlatformSensor> sensor) {
            EXPECT_FALSE(sensor);
            run_loop.Quit();
          });

  // PlatformSensorProviderWinrt is not implemented so it should always
  // return nullptr.
  provider->CreateSensor(mojom::SensorType::AMBIENT_LIGHT,
                         CreateSensorCallback);
  run_loop.Run();
}

}  // namespace device