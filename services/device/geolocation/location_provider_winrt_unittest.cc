// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_winrt.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

class MockLocationObserver {
 public:
  MockLocationObserver() = default;
  ~MockLocationObserver() = default;

  void InvalidateLastPosition() {
    last_position_.error_code = mojom::Geoposition::ErrorCode::NONE;
    EXPECT_FALSE(ValidateGeoposition(last_position_));
  }

  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& position) {
    last_position_ = position;
    on_location_update_called_ = true;
  }

  mojom::Geoposition last_position() { return last_position_; }

  bool on_location_update_called() { return on_location_update_called_; }

 private:
  mojom::Geoposition last_position_;
  bool on_location_update_called_ = false;
};

}  // namespace

class LocationProviderWinrtTest : public testing::Test {
 protected:
  LocationProviderWinrtTest()
      : observer_(std::make_unique<MockLocationObserver>()),
        callback_(base::BindRepeating(&MockLocationObserver::OnLocationUpdate,
                                      base::Unretained(observer_.get()))) {}

  void InitializeProvider() {
    provider_ = std::make_unique<LocationProviderWinrt>();
    provider_->SetUpdateCallback(callback_);
  }

  // testing::Test
  void TearDown() override {}

  base::test::TaskEnvironment task_environment_;
  const std::unique_ptr<MockLocationObserver> observer_;
  const LocationProvider::LocationProviderUpdateCallback callback_;
  std::unique_ptr<LocationProviderWinrt> provider_;
};

TEST_F(LocationProviderWinrtTest, CreateDestroy) {
  InitializeProvider();
  EXPECT_TRUE(provider_);
  provider_.reset();
}

// Tests when OnPermissionGranted() called location update is provided.
TEST_F(LocationProviderWinrtTest, HasPermissions) {
  InitializeProvider();
  provider_->OnPermissionGranted();
  provider_->StartProvider(/*enable_high_accuracy=*/false);

  bool on_location_update_called = observer_->on_location_update_called();
  EXPECT_TRUE(on_location_update_called);

  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));
  EXPECT_EQ(mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE,
            observer_->last_position().error_code);
}

// Tests when OnPermissionGranted() has not been called location update
// is not provided.
TEST_F(LocationProviderWinrtTest, NoPermissions) {
  InitializeProvider();
  provider_->StartProvider(/*enable_high_accuracy=*/false);

  EXPECT_FALSE(observer_->on_location_update_called());
}
}  // namespace device
