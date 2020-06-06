// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

static const base::TickClock* g_clock;

class TestClient : public VisibilityMetricsLogger::Client {
 public:
  explicit TestClient(VisibilityMetricsLogger* logger) : logger_(logger) {
    logger_->AddClient(this);
  }

  virtual ~TestClient() { logger_->RemoveClient(this); }

  void SetViewAttached(bool view_attached) {
    visibility_info_.view_attached = view_attached;
    logger_->ClientVisibilityChanged(this);
  }

  void SetViewVisible(bool view_visible) {
    visibility_info_.view_visible = view_visible;
    logger_->ClientVisibilityChanged(this);
  }

  void SetWindowVisible(bool window_visible) {
    visibility_info_.window_visible = window_visible;
    logger_->ClientVisibilityChanged(this);
  }

  // VisibilityMetricsLogger::Client implementation
  VisibilityMetricsLogger::VisibilityInfo GetVisibilityInfo() override {
    return visibility_info_;
  }

 private:
  VisibilityMetricsLogger* logger_;
  VisibilityMetricsLogger::VisibilityInfo visibility_info_;
};

class VisibilityMetricsLoggerTest : public testing::Test {
 public:
  VisibilityMetricsLoggerTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    g_clock = task_environment_.GetMockTickClock();
  }

  ~VisibilityMetricsLoggerTest() override { g_clock = nullptr; }

  VisibilityMetricsLogger* logger() { return logger_.get(); }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  // testing::Test.
  void SetUp() override {
    logger_ = std::make_unique<VisibilityMetricsLogger>();
  }

  void TearDown() override {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<VisibilityMetricsLogger> logger_;
};

TEST_F(VisibilityMetricsLoggerTest, TestFractionalSecondAccumulation) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);

  task_environment().FastForwardBy(base::TimeDelta::FromMilliseconds(500));

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 0);

  task_environment().FastForwardBy(base::TimeDelta::FromMilliseconds(500));

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 1);

  client.reset();
}

TEST_F(VisibilityMetricsLoggerTest, TestSingleVisibleClient) {
  base::HistogramTester histogram_tester;

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(10));
  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(30));
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(10));
  client->SetWindowVisible(false);

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 10);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 40);

  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(90));

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 100);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 40);

  client.reset();
}

TEST_F(VisibilityMetricsLoggerTest, TestLongDurationVisibleClient) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(300));
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(50));
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(50));
  client2.reset();

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(50));
  client1.reset();

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 150);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 300);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kVisible, 200);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kNotVisible, 650);
}

TEST_F(VisibilityMetricsLoggerTest, TestTwoVisibleClients) {
  // t=0: client1 created
  // t=10: client2 created
  // t=40: client1 visible, recording scheduled for t+60s
  // t=50: client2 visible
  // t=60: client1 invisible
  // t=70: client2 invisible
  // t=100: clients deleted.

  // Time with any client visible: 70 - 40 = 30
  // Time with no visible client: 100 - 30 = 70
  // Time x visible clients: (50-40) * 1 + (60-50) * 2 + (70-60) * 1 = 40
  // Time x hidden clients: 100 + 90 - 40 = 150
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(10));
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(30));
  // This queues delayed recording after 60 seconds (test-defined)
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(10));
  // No additional task is queued
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(10));
  // This does not cause metrics to be recorded because one client remains
  // visible.
  client1->SetWindowVisible(false);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(10));
  // The last client becoming invisible triggers immediate recording and the
  // cancellation of the queued task.
  client2->SetWindowVisible(false);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(30));
  client1.reset();
  client2.reset();

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 30);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 70);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kVisible, 40);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kNotVisible, 150);
}

}  // namespace android_webview
