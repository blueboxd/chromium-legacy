// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"

namespace arc {

namespace {

constexpr char kFocusAppPackage[] = "focus.app.package";
constexpr char kFocusAppActivity[] = "focus.app.package.Activity";
constexpr char kFocusCategory[] = "OnlineGame";
constexpr char kNonFocusAppPackage[] = "nonfocus.app.package";
constexpr char kNonFocusAppActivity[] = "nonfocus.app.package.Activity";
// For 20 frames.
constexpr base::TimeDelta kTestPeriod =
    base::TimeDelta::FromSeconds(1) / (60 / 20);

// Creates app window as ARC++ window.
views::Widget* CreateArcWindow(const std::string& window_app_id) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  params.context = nullptr;
  views::Widget* widget = new views::Widget();
  widget->Init(params);
  // Set ARC id before showing the window to be recognized in
  // ArcAppWindowLauncherController.
  exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
  exo::SetShellMainSurface(widget->GetNativeWindow(), new exo::Surface());
  widget->Show();
  widget->Activate();
  return widget;
}

// Creates name of histogram with required statistics.
std::string GetFocusStatisticName(const std::string& name) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            kFocusCategory);
}

// Reads statistics value from histogram.
int64_t ReadFocusStatistics(const std::string& name) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(GetFocusStatisticName(name));
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotFinalDelta();
  DCHECK(samples.get());
  DCHECK_EQ(1, samples->TotalCount());
  return samples->sum();
}

}  // namespace

// BrowserWithTestWindowTest contains required ash/shell support that would not
// be possible to use directly.
class ArcAppPerformanceTracingTest : public BrowserWithTestWindowTest {
 public:
  ArcAppPerformanceTracingTest() = default;
  ~ArcAppPerformanceTracingTest() override = default;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    wm_helper_ = std::make_unique<exo::WMHelperChromeOS>();
    exo::WMHelper::SetInstance(wm_helper_.get());

    arc_test_.SetUp(profile());

    ArcAppPerformanceTracing::SetFocusAppForTesting(
        kFocusAppPackage, kFocusAppActivity, kFocusCategory);

    performance_tracing_ = std::make_unique<ArcAppPerformanceTracing>(
        profile(), nullptr /* bridge, unused */);

    performance_tracing_->SetTracingPeriodForTesting(kTestPeriod);
  }

  void TearDown() override {
    performance_tracing_->Shutdown();
    performance_tracing_.reset();
    arc_test_.TearDown();
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Ensures that tracing is active.
  void StartArcFocusAppTracing() {
    views::Widget* const arc_widget = CreateArcWindow("org.chromium.arc.1");
    DCHECK(arc_widget && arc_widget->GetNativeWindow());
    performance_tracing().OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
    performance_tracing().OnTaskCreated(1 /* task_Id */, kFocusAppPackage,
                                        kFocusAppActivity,
                                        std::string() /* intent */);
    DCHECK(performance_tracing().session());
    DCHECK(!performance_tracing().session()->tracing_active());
    performance_tracing().session()->FireTimerForTesting();
    DCHECK(performance_tracing().session());
    DCHECK(performance_tracing().session()->tracing_active());
  }

  // Sends sequence of commits where each commit is delayed for specific delta
  // from |deltas|.
  void PlaySequence(const std::vector<base::TimeDelta>& deltas) {
    DCHECK(performance_tracing().session());
    DCHECK(performance_tracing().session()->tracing_active());
    base::Time timestamp = base::Time::Now();
    performance_tracing().session()->OnCommitForTesting(timestamp);
    for (const base::TimeDelta& delta : deltas) {
      timestamp += delta;
      performance_tracing().session()->OnCommitForTesting(timestamp);
    }
    // Fire timer at the end to finish statistics tracing.
    performance_tracing().session()->FireTimerForTesting();
  }

  ArcAppPerformanceTracing& performance_tracing() {
    return *performance_tracing_;
  }

 private:
  ArcAppTest arc_test_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<ArcAppPerformanceTracing> performance_tracing_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracingTest);
};

TEST_F(ArcAppPerformanceTracingTest, TracingScheduled) {
  // By default it is inactive.
  EXPECT_FALSE(performance_tracing().session());

  // Report task first.
  performance_tracing().OnTaskCreated(1 /* task_Id */, kFocusAppPackage,
                                      kFocusAppActivity,
                                      std::string() /* intent */);
  EXPECT_FALSE(performance_tracing().session());

  // Create window second.
  views::Widget* const arc_widget1 = CreateArcWindow("org.chromium.arc.1");
  ASSERT_TRUE(arc_widget1);
  ASSERT_TRUE(arc_widget1->GetNativeWindow());
  performance_tracing().OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget1->GetNativeWindow(), nullptr /* lost_active */);
  ASSERT_TRUE(performance_tracing().session());
  EXPECT_FALSE(performance_tracing().session()->tracing_active());

  // Test reverse order, create window first.
  views::Widget* const arc_widget2 = CreateArcWindow("org.chromium.arc.2");
  ASSERT_TRUE(arc_widget2);
  ASSERT_TRUE(arc_widget2->GetNativeWindow());
  performance_tracing().OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget2->GetNativeWindow(), arc_widget2->GetNativeWindow());
  // Task is not yet created, this also resets previous tracing.
  EXPECT_FALSE(performance_tracing().session());
  // Report task second.
  performance_tracing().OnTaskCreated(2 /* task_Id */, kFocusAppPackage,
                                      kFocusAppActivity,
                                      std::string() /* intent */);
  ASSERT_TRUE(performance_tracing().session());
  EXPECT_FALSE(performance_tracing().session()->tracing_active());
}

TEST_F(ArcAppPerformanceTracingTest, TracingNotScheduledForNonFocusApp) {
  views::Widget* const arc_widget = CreateArcWindow("org.chromium.arc.1");
  ASSERT_TRUE(arc_widget);
  ASSERT_TRUE(arc_widget->GetNativeWindow());
  performance_tracing().OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  EXPECT_FALSE(performance_tracing().session());
  performance_tracing().OnTaskCreated(1 /* task_Id */, kNonFocusAppPackage,
                                      kNonFocusAppActivity,
                                      std::string() /* intent */);
  EXPECT_FALSE(performance_tracing().session());
}

TEST_F(ArcAppPerformanceTracingTest, TracingStoppedOnIdle) {
  StartArcFocusAppTracing();
  const base::TimeDelta normal_interval = base::TimeDelta::FromSeconds(1) / 60;
  base::Time timestamp = base::Time::Now();
  performance_tracing().session()->OnCommitForTesting(timestamp);
  // Expected updates;
  timestamp += normal_interval;
  performance_tracing().session()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(performance_tracing().session());
  EXPECT_TRUE(performance_tracing().session()->tracing_active());

  timestamp += normal_interval * 5;
  performance_tracing().session()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(performance_tracing().session());
  EXPECT_TRUE(performance_tracing().session()->tracing_active());

  // Too long update.
  timestamp += normal_interval * 10;
  performance_tracing().session()->OnCommitForTesting(timestamp);
  // Tracing is rescheduled and no longer active.
  ASSERT_TRUE(performance_tracing().session());
  EXPECT_FALSE(performance_tracing().session()->tracing_active());
}

TEST_F(ArcAppPerformanceTracingTest, StatisticsReported) {
  StartArcFocusAppTracing();
  const base::TimeDelta normal_interval = base::TimeDelta::FromSeconds(1) / 60;
  const base::TimeDelta error1 = base::TimeDelta::FromMicroseconds(100);
  const base::TimeDelta error2 = base::TimeDelta::FromMicroseconds(200);
  const base::TimeDelta error3 = base::TimeDelta::FromMicroseconds(300);
  const std::vector<base::TimeDelta> sequence = {
      normal_interval + error1,
      normal_interval + error2,
      // One frame skip
      normal_interval * 2 + error3,
      normal_interval - error1,
      normal_interval - error2,
      // Two frames skip
      normal_interval * 3 - error3,
      normal_interval + error1,
      normal_interval + error2,
      normal_interval * 2 + error3,
      normal_interval - error1,
      normal_interval * 2 - error2,
      normal_interval - error3,
      normal_interval + error1,
      normal_interval + error2,
      normal_interval + error3,
  };
  EXPECT_FALSE(performance_tracing().WasReported(kFocusCategory));
  PlaySequence(sequence);
  EXPECT_TRUE(performance_tracing().WasReported(kFocusCategory));
  EXPECT_EQ(45L, ReadFocusStatistics("FPS"));
  EXPECT_EQ(216L, ReadFocusStatistics("CommitDeviation"));
  EXPECT_EQ(48L, ReadFocusStatistics("RenderQuality"));
}

}  // namespace arc
