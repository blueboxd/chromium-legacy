// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient() {}
  ~TestClient() override {}

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }

  // Expose the super class implementation for testing.
  using AwMetricsServiceClient::GetAppPackageNameInternal;

 protected:
  int GetSampleRatePerMille() override { return 1000; }
  int GetPackageNameLimitRatePerMille() override { return 1000; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  AwMetricsServiceClient::RegisterPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  return client;
}

}  // namespace

class AwMetricsServiceClientTest : public testing::Test {
 public:
  AwMetricsServiceClientTest() : task_runner_(new base::TestSimpleTaskRunner) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
  }

 protected:
  ~AwMetricsServiceClientTest() override {}

  void SetUp() override {
    test_content_client_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    test_content_client_initializer_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClientTest);
};

// TODO(https://crbug.com/995544): remove this when the
// kMetricsReportingEnabledTimestamp pref has been persisted for one or two
// milestones.
TEST_F(AwMetricsServiceClientTest, TestBackfillEnabledDateIfMissing) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_TRUE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
TEST_F(AwMetricsServiceClientTest, TestPreferPersistedInstallDate) {
  base::HistogramTester histogram_tester;
  auto prefs = CreateTestPrefs();
  int64_t install_date = 12345;
  prefs->SetInt64(metrics::prefs::kInstallDate, install_date);
  auto client = CreateAndInitTestClient(prefs.get());
  EXPECT_EQ(install_date, prefs->GetInt64(metrics::prefs::kInstallDate));

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kValidInstallDatePref, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.BackfillInstallDate", 1);
}

// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
TEST_F(AwMetricsServiceClientTest, TestGetInstallDateFromJavaIfMissing) {
  base::HistogramTester histogram_tester;
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  // All we can safely assert is the install time is set, since checking the
  // actual time is racy (ex. in the unlikely scenario if this test executes in
  // the same millisecond as when the package was installed).
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kInstallDate));

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kPersistedPackageManagerInstallDate, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.BackfillInstallDate", 1);
}

}  // namespace android_webview
