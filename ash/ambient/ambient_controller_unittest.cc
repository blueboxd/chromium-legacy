// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/system/power/power_status.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::TimeDelta::FromHours(1);

}  // namespace

using AmbientControllerTest = AmbientAshTestBase;

TEST_F(AmbientControllerTest, ShowAmbientScreenUponLock) {
  LockScreen();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, HideAmbientScreen) {
  LockScreen();
  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  HideAmbientScreen();

  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kHidden);
  EXPECT_FALSE(container_view()->GetWidget()->IsVisible());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, CloseAmbientScreenUponUnlock) {
  LockScreen();
  EXPECT_TRUE(container_view());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  UnlockScreen();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  EXPECT_FALSE(container_view());
}

TEST_F(AmbientControllerTest, ShouldRequestAccessTokenWhenLockingScreen) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Should close ambient widget already when unlocking screen.
  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldReturnCachedAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, access_token);

        std::move(closure).Run();
        run_loop.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop.Run();

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRefreshAccessTokenAfterFailure) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Token request automatically retry.
  // The failure delay has jitter so fast forward a bit more, but before
  // the returned token would expire again.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay / 2);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryIsCharging) {
  // Flush the loop first to ensure the |PowerStatus| has picked up the initial
  // status.
  base::RunLoop().RunUntilIdle();

  // Simulate a device being connected to a charger initially.
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  PowerStatus::Get()->SetProtoForTesting(proto);

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // TODO(meilinw): refactor |AmbientAshTestBase| to make this built-in.
  // Simulate the ambient screen being shown again.
  ambient_controller()->OnAmbientUiVisibilityChanged(
      AmbientUiVisibility::kShown);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryStateChanged) {
  // Flush the loop first to ensure the |PowerStatus| has picked up the initial
  // status.
  base::RunLoop().RunUntilIdle();

  // Simulate a device being disconnected with a charger initially.
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(proto);
  // Lock screen to start ambient mode.
  LockScreen();

  // Should not acquire wake lock when device is not charging.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Connect the device with a charger.
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto);
  // Notify the controller about the power status change, and flush the loop to
  // ensure the wake lock request has reached the wake lock provider.
  ambient_controller()->OnPowerStatusChanged();
  base::RunLoop().RunUntilIdle();

  // Should acquire the wake lock when battery is charging.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates a full battery.
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();

  // Should keep the wake lock as the charger is still connected.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Disconnects the charger again.
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when battery is not charging.
  EXPECT_TRUE(ambient_controller()->IsShown());
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // An unbalanced release should do nothing.
  UnlockScreen();
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest, ShouldDismissContainerViewWhenKeyPressed) {
  ShowAmbientScreen();
  EXPECT_TRUE(container_view()->GetWidget()->IsVisible());

  // Simulates a random keyboard press event.
  GetEventGenerator()->PressKey(ui::VKEY_SPACE, /*flags=*/0);

  EXPECT_FALSE(container_view()->GetWidget()->IsVisible());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, UpdateUiAndWakeLockWhenSystemSuspendOrResume) {
  // Flush the loop first to ensure the |PowerStatus| has picked up the initial
  // status.
  base::RunLoop().RunUntilIdle();

  // Simulate a device being connected with a charger initially to acquire a
  // wake lock upon shown.
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  PowerStatus::Get()->SetProtoForTesting(proto);

  // Starts ambient screen.
  ShowAmbientScreen();
  EXPECT_TRUE(ambient_controller()->IsShown());
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates the system starting to suspend by lid closed.
  SimulateSystemSuspendAndWait(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  // System suspension should hide Ui and release the wake lock acquired
  // previously.
  EXPECT_FALSE(container_view()->GetWidget()->IsVisible());
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates the system starting to resume.
  SimulateSystemResumeAndWait();

  // System resume should not invoke Ui to show up, thus no wake lock needed
  // to acquire.
  EXPECT_FALSE(container_view()->GetWidget()->IsVisible());
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

}  // namespace ash
