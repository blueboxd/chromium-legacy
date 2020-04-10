// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_manager_impl.h"

#include <stdint.h>
#include <memory>

#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Mock reporter that tracks reports being queued by the ConversionManager.
class TestConversionReporter
    : public ConversionManagerImpl::ConversionReporter {
 public:
  TestConversionReporter() = default;
  ~TestConversionReporter() override = default;

  // ConversionManagerImpl::ConversionReporter
  void AddReportsToQueue(std::vector<ConversionReport> reports) override {
    num_reports_ += reports.size();
    last_conversion_id_ = *reports.back().conversion_id;

    if (quit_closure_ && num_reports_ >= expected_num_reports_)
      std::move(quit_closure_).Run();
  }

  size_t num_reports() { return num_reports_; }

  int64_t last_conversion_id() { return last_conversion_id_; }

  void WaitForNumReports(size_t expected_num_reports) {
    if (num_reports_ >= expected_num_reports)
      return;

    expected_num_reports_ = expected_num_reports;
    base::RunLoop wait_loop;
    quit_closure_ = wait_loop.QuitClosure();
    wait_loop.Run();
    ;
  }

 private:
  size_t expected_num_reports_ = 0u;
  size_t num_reports_ = 0u;
  int64_t last_conversion_id_ = 0UL;
  base::OnceClosure quit_closure_;
};

// Time after impression that a conversion can first be sent. See
// ConversionStorageDelegateImpl::GetReportTimeForConversion().
constexpr base::TimeDelta kFirstReportingWindow = base::TimeDelta::FromDays(2);

// Give impressions a sufficiently long expiry.
constexpr base::TimeDelta kImpressionExpiry = base::TimeDelta::FromDays(30);

}  // namespace

class ConversionManagerImplTest : public testing::Test {
 public:
  ConversionManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    CreateManager();
  }

  void CreateManager() {
    auto reporter = std::make_unique<TestConversionReporter>();
    test_reporter_ = reporter.get();
    conversion_manager_ = ConversionManagerImpl::CreateForTesting(
        std::move(reporter), task_environment_.GetMockClock(), dir_.GetPath(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ConversionManagerImpl> conversion_manager_;
  TestConversionReporter* test_reporter_ = nullptr;
};

TEST_F(ConversionManagerImplTest, ImpressionConverted_ReportQueued) {
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());

  // Reports are queued in intervals ahead of when they should be
  // sent. Make sure the report is not queued earlier than this.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kConversionManagerQueueReportsInterval -
                                  base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(0u, test_reporter_->num_reports());

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

TEST_F(ConversionManagerImplTest, QueuedReportNotSent_QueuedAgain) {
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kConversionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // If the report is not sent, it should be added to the queue again.
  task_environment_.FastForwardBy(kConversionManagerQueueReportsInterval);
  EXPECT_EQ(2u, test_reporter_->num_reports());
}

TEST_F(ConversionManagerImplTest, QueuedReportSent_NotQueuedAgain) {
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kConversionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // Notify the manager that the report has been sent.
  conversion_manager_->HandleSentReport(test_reporter_->last_conversion_id());

  // The report should not be added to the queue again.
  task_environment_.FastForwardBy(kConversionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

// Add a conversion to storage and reset the manager to mimic a report being
// available at startup.
TEST_F(ConversionManagerImplTest, ExpiredReportsAtStartup_Queued) {
  // Create a report that will be reported at t= 2 days.
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());

  // Create another conversion that will be reported at t=
  // (kFirstReportingWindow + 2 * kConversionManagerQueueReportsInterval).
  task_environment_.FastForwardBy(2 * kConversionManagerQueueReportsInterval);
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());

  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  conversion_manager_.reset();

  // Fast forward past the expected report time of the first conversion, t =
  // (kFirstReportingWindow+ 1 minute).
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  (2 * kConversionManagerQueueReportsInterval) +
                                  base::TimeDelta::FromMinutes(1));

  // Create the manager and check that the first report is queued immediately.
  CreateManager();
  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());
  conversion_manager_->HandleSentReport(test_reporter_->last_conversion_id());

  // The second report is still queued at the correct time.
  task_environment_.FastForwardBy(kConversionManagerQueueReportsInterval);
  EXPECT_EQ(2u, test_reporter_->num_reports());
}

}  // namespace content
