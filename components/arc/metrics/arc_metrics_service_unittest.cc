// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#include "components/arc/test/test_browser_context.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace arc {
namespace {

// Helper class that initializes and shuts down dbus clients for testing.
class DBusThreadManagerLifetimeHelper {
 public:
  DBusThreadManagerLifetimeHelper() {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::SessionManagerClient::InitializeFakeInMemory();
  }

  ~DBusThreadManagerLifetimeHelper() {
    chromeos::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }
};

// Helper class that ensures lifetime of StabilityMetricsManager for testing.
class ScopedStabilityMetricsManager {
 public:
  ScopedStabilityMetricsManager() {
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    StabilityMetricsManager::Initialize(&local_state_);
  }

  ~ScopedStabilityMetricsManager() { StabilityMetricsManager::Shutdown(); }

 private:
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedStabilityMetricsManager);
};

// Initializes dependencies before creating ArcMetricsService instance.
ArcMetricsService* CreateArcMetricsService(TestBrowserContext* context) {
  // Register preferences for ARC++ engagement time metrics.
  prefs::RegisterProfilePrefs(context->pref_registry());

  return ArcMetricsService::GetForBrowserContextForTesting(context);
}

// The event names the container sends to Chrome.
constexpr std::array<const char*, 11> kBootEvents{
    "boot_progress_start",
    "boot_progress_preload_start",
    "boot_progress_preload_end",
    "boot_progress_system_run",
    "boot_progress_pms_start",
    "boot_progress_pms_system_scan_start",
    "boot_progress_pms_data_scan_start",
    "boot_progress_pms_scan_end",
    "boot_progress_pms_ready",
    "boot_progress_ams_ready",
    "boot_progress_enable_screen"};

class ArcMetricsServiceTest : public testing::Test {
 protected:
  ArcMetricsServiceTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        context_(std::make_unique<TestBrowserContext>()),
        service_(CreateArcMetricsService(context_.get())) {
    chromeos::FakeSessionManagerClient::Get()->set_arc_available(true);

    CreateFakeWindows();
    service_->SetWindowMatcherForTesting(base::BindRepeating(
        &ArcMetricsServiceTest::MatchWindow, base::Unretained(this)));
    // The code doesn't work for correctly for a clock at the epoch.
    test_clock_.SetNow(base::Time::Now());
    service_->SetClockForTesting(&test_clock_);
    service_->SetTickClockForTesting(&test_tick_clock_);
  }

  ~ArcMetricsServiceTest() override {}

  ArcMetricsService* service() { return service_; }

  void SetArcStartTimeInMs(uint64_t arc_start_time_in_ms) {
    const base::TimeTicks arc_start_time =
        base::TimeDelta::FromMilliseconds(arc_start_time_in_ms) +
        base::TimeTicks();
    chromeos::FakeSessionManagerClient::Get()->set_arc_start_time(
        arc_start_time);
  }

  std::vector<mojom::BootProgressEventPtr> GetBootProgressEvents(
      uint64_t start_in_ms,
      uint64_t step_in_ms) {
    std::vector<mojom::BootProgressEventPtr> events;
    for (size_t i = 0; i < kBootEvents.size(); ++i) {
      events.emplace_back(mojom::BootProgressEvent::New(
          kBootEvents[i], start_in_ms + (step_in_ms * i)));
    }
    return events;
  }

  void SetSessionState(session_manager::SessionState state) {
    session_manager_.SetSessionState(state);
  }

  void SetScreenDimmed(bool is_screen_dimmed) {
    power_manager::ScreenIdleState screen_idle_state;
    screen_idle_state.set_dimmed(is_screen_dimmed);
    GetPowerManagerClient()->SendScreenIdleStateChanged(screen_idle_state);
  }

  void TriggerRecordEngagementTimeToUma() {
    // Trigger UMA record by changing to next day.
    test_clock_.Advance(base::TimeDelta::FromDays(1));
    service_->OnSessionStateChanged();
  }

  aura::Window* fake_arc_window() { return fake_arc_window_.get(); }
  aura::Window* fake_non_arc_window() { return fake_non_arc_window_.get(); }

  void AdvanceSeconds(int seconds) {
    test_tick_clock_.Advance(base::TimeDelta::FromSeconds(seconds));
  }

 private:
  void CreateFakeWindows() {
    fake_arc_window_.reset(aura::test::CreateTestWindowWithId(
        /*id=*/0, nullptr));
    fake_arc_window_->SetProperty(aura::client::kAppType,
                                  static_cast<int>(ash::AppType::ARC_APP));
    fake_non_arc_window_.reset(aura::test::CreateTestWindowWithId(
        /*id=*/1, nullptr));
  }

  bool MatchWindow(const aura::Window* window) const {
    return window == fake_arc_window_.get();
  }

  chromeos::FakePowerManagerClient* GetPowerManagerClient() {
    return static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::PowerManagerClient::Get());
  }

  content::BrowserTaskEnvironment task_environment_;

  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;

  std::unique_ptr<ArcServiceManager> arc_service_manager_;

  // DBusThreadManager, SessionManager and StabilityMetricsManager should
  // outlive TestBrowserContext which destructs ArcMetricsService in dtor.
  DBusThreadManagerLifetimeHelper dbus_thread_manager_lifetime_helper_;
  session_manager::SessionManager session_manager_;
  ScopedStabilityMetricsManager scoped_stability_metrics_manager_;
  std::unique_ptr<TestBrowserContext> context_;

  std::unique_ptr<aura::Window> fake_arc_window_;
  std::unique_ptr<aura::Window> fake_non_arc_window_;

  ArcMetricsService* const service_;

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsServiceTest);
};

// Tests that ReportBootProgress() actually records UMA stats.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_FirstBoot) {
  // Start the full ARC container at t=10. Also set boot_progress_start to 10,
  // boot_progress_preload_start to 11, and so on.
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));

  // Call ReportBootProgress() and then confirm that
  // Arc.boot_progress_start.FirstBoot is recorded with 0 (ms),
  // Arc.boot_progress_preload_start.FirstBoot is with 1 (ms), etc.
  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::FIRST_BOOT);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    tester.ExpectUniqueSample(
        std::string("Arc.") + kBootEvents[i] + ".FirstBoot", i,
        1 /* count of the sample */);
  }
  // Confirm that Arc.AndroidBootTime.FirstBoot is also recorded, and has the
  // same value as "Arc.boot_progress_enable_screen.FirstBoot".
  std::unique_ptr<base::HistogramSamples> samples =
      tester.GetHistogramSamplesSinceCreation(
          "Arc." + std::string(kBootEvents.back()) + ".FirstBoot");
  ASSERT_TRUE(samples.get());
  tester.ExpectUniqueSample("Arc.AndroidBootTime.FirstBoot", samples->sum(), 1);
}

// Does the same but with negative values and FIRST_BOOT_AFTER_UPDATE.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_FirstBootAfterUpdate) {
  // Start the full ARC container at t=10. Also set boot_progress_start to 5,
  // boot_progress_preload_start to 7, and so on. This can actually happen
  // because the mini container can finish up to boot_progress_preload_end
  // before the full container is started.
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs - 5, 2 /* step_in_ms */));

  // Call ReportBootProgress() and then confirm that
  // Arc.boot_progress_start.FirstBoot is recorded with 0 (ms),
  // Arc.boot_progress_preload_start.FirstBoot is with 0 (ms), etc. Unlike our
  // performance dashboard where negative performance numbers are treated as-is,
  // UMA treats them as zeros.
  base::HistogramTester tester;
  // This time, use mojom::BootType::FIRST_BOOT_AFTER_UPDATE.
  service()->ReportBootProgress(std::move(events),
                                mojom::BootType::FIRST_BOOT_AFTER_UPDATE);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    const int expected = std::max<int>(0, i * 2 - 5);
    tester.ExpectUniqueSample(
        std::string("Arc.") + kBootEvents[i] + ".FirstBootAfterUpdate",
        expected, 1);
  }
  std::unique_ptr<base::HistogramSamples> samples =
      tester.GetHistogramSamplesSinceCreation(
          "Arc." + std::string(kBootEvents.back()) + ".FirstBootAfterUpdate");
  ASSERT_TRUE(samples.get());
  tester.ExpectUniqueSample("Arc.AndroidBootTime.FirstBootAfterUpdate",
                            samples->sum(), 1);
}

// Does the same but with REGULAR_BOOT.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_RegularBoot) {
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs - 5, 2 /* step_in_ms */));

  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events),
                                mojom::BootType::REGULAR_BOOT);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    const int expected = std::max<int>(0, i * 2 - 5);
    tester.ExpectUniqueSample(
        std::string("Arc.") + kBootEvents[i] + ".RegularBoot", expected, 1);
  }
  std::unique_ptr<base::HistogramSamples> samples =
      tester.GetHistogramSamplesSinceCreation(
          "Arc." + std::string(kBootEvents.back()) + ".RegularBoot");
  ASSERT_TRUE(samples.get());
  tester.ExpectUniqueSample("Arc.AndroidBootTime.RegularBoot", samples->sum(),
                            1);
}

// Tests that no UMA is recorded when nothing is reported.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_EmptyResults) {
  SetArcStartTimeInMs(100);
  std::vector<mojom::BootProgressEventPtr> events;  // empty

  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::FIRST_BOOT);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    tester.ExpectTotalCount(std::string("Arc.") + kBootEvents[i] + ".FirstBoot",
                            0);
  }
  tester.ExpectTotalCount("Arc.AndroidBootTime.FirstBoot", 0);
}

// Tests that no UMA is recorded when BootType is invalid.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_InvalidBootType) {
  SetArcStartTimeInMs(100);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(123, 456));
  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::UNKNOWN);
  base::RunLoop().RunUntilIdle();
  for (const std::string& suffix :
       {".FirstBoot", ".FirstBootAfterUpdate", ".RegularBoot"}) {
    tester.ExpectTotalCount("Arc." + (kBootEvents.front() + suffix), 0);
    tester.ExpectTotalCount("Arc." + (kBootEvents.back() + suffix), 0);
    tester.ExpectTotalCount("Arc.AndroidBootTime" + suffix, 0);
  }
}

TEST_F(ArcMetricsServiceTest, ReportNativeBridge) {
  // SetArcNativeBridgeType should be called once ArcMetricsService is
  // constructed.
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::UNKNOWN);
  service()->ReportNativeBridge(mojom::NativeBridgeType::NONE);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::NONE);
  service()->ReportNativeBridge(mojom::NativeBridgeType::HOUDINI);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::HOUDINI);
  service()->ReportNativeBridge(mojom::NativeBridgeType::NDK_TRANSLATION);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::NDK_TRANSLATION);
}

TEST_F(ArcMetricsServiceTest, RecordArcWindowFocusAction) {
  base::HistogramTester tester;

  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);

  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);
}

TEST_F(ArcMetricsServiceTest, RecordNothingNonArcWindowFocusAction) {
  base::HistogramTester tester;

  // Focus an ARC window once so that the histogram is created.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);

  // Focusing a non-ARC window should not increase the bucket count.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_non_arc_window(), nullptr);

  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);
}

TEST_F(ArcMetricsServiceTest, RecordEngagementTimeSessionLocked) {
  base::HistogramTester tester;

  // Make session inactive for 1 sec. Nothing should be recorded.
  SetSessionState(session_manager::SessionState::LOCKED);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(0), 1);
}

TEST_F(ArcMetricsServiceTest, RecordEngagementTimeSessionActive) {
  base::HistogramTester tester;

  // Make session active for 1 sec. Should be recorded as total time.
  SetSessionState(session_manager::SessionState::ACTIVE);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(0), 1);
}

TEST_F(ArcMetricsServiceTest, RecordEngagementTimeScreenDimmed) {
  base::HistogramTester tester;
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Dim screen off for 1 sec. Nothing should be recorded.
  SetScreenDimmed(true);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(0), 1);
}

TEST_F(ArcMetricsServiceTest, RecordEngagementTimeArcWindowFocused) {
  base::HistogramTester tester;
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Focus an ARC++ window for 1 sec. Should be recorded as total time and
  // foreground time.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(0), 1);
}

TEST_F(ArcMetricsServiceTest, RecordEngagementTimeNonArcWindowFocused) {
  base::HistogramTester tester;
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Focus an non-ARC++ window for 1 sec. Should be recorded as total time.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_non_arc_window(), nullptr);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(0), 1);
}

TEST_F(ArcMetricsServiceTest, RecordEngagementTimeAppInBackground) {
  base::HistogramTester tester;
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Open an ARC++ app in the background and wait for 1 sec. Should be recorded
  // as total time and background time.
  service()->OnTaskCreated(1, "", "", "");
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(0), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(1), 1);
}

TEST_F(ArcMetricsServiceTest,
       RecordEngagementTimeAppInBackgroundAndArcWindowFocused) {
  base::HistogramTester tester;
  SetSessionState(session_manager::SessionState::ACTIVE);

  // With an ARC++ app in the background, focus an ARC++ window for 1 sec.
  // Should be recorded as total time and foreground time.
  service()->OnTaskCreated(1, "", "", "");
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Total",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.ArcTotal",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Foreground",
                               base::TimeDelta::FromSeconds(1), 1);
  tester.ExpectTimeBucketCount("Arc.EngagementTime.Background",
                               base::TimeDelta::FromSeconds(0), 1);
}

}  // namespace
}  // namespace arc
