// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_metrics_service_client.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient() : in_sample_(true) {}
  ~TestClient() override {}

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }
  void SetInSample(bool value) { in_sample_ = value; }

 protected:
  bool IsInSample() override { return true; }

 private:
  bool in_sample_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  metrics::MetricsService::RegisterPrefs(prefs->registry());
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

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClientTest);
};

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true);
  client->Initialize(prefs.get());
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false);
  client->Initialize(prefs.get());
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true);
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false);
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

// If there is already a valid client ID, it should be reused.
TEST_F(AwMetricsServiceClientTest, TestKeepExistingClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true);
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  ASSERT_EQ(kTestClientId, prefs->GetString(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseClearsClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false);
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetrics) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  // Flip everything to false; flag should have higher precedence.
  client->SetHaveMetricsConsent(false);
  client->SetInSample(false);
  ASSERT_TRUE(client->IsReportingEnabled());

  // Flip things to true just to double-check everything still works.
  client->SetHaveMetricsConsent(true);
  client->SetInSample(true);
  ASSERT_TRUE(client->IsReportingEnabled());
}

}  // namespace android_webview
