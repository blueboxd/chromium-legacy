// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_cleaner.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/history/test_print_job_database.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using printing::proto::PrintJobInfo;

namespace {

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";

PrintJobInfo ConstructPrintJobInfo(const std::string& id,
                                   const base::Time& completion_time) {
  PrintJobInfo print_job_info;
  print_job_info.set_id(id);
  print_job_info.set_completion_time(
      static_cast<int64_t>(completion_time.ToJsTime()));
  return print_job_info;
}

}  // namespace

class PrintJobHistoryCleanerTest : public ::testing::Test {
 public:
  PrintJobHistoryCleanerTest() = default;

  void SetUp() override {
    test_prefs_.SetInitializationCompleted();

    print_job_database_ = std::make_unique<TestPrintJobDatabase>();
    print_job_history_cleaner_ = std::make_unique<PrintJobHistoryCleaner>(
        print_job_database_.get(), &test_prefs_);

    auto timer = std::make_unique<base::RepeatingTimer>(
        task_environment_.GetMockTickClock());
    timer->SetTaskRunner(task_environment_.GetMainThreadTaskRunner());
    print_job_history_cleaner_->OverrideTimeForTesting(
        task_environment_.GetMockClock(), std::move(timer));

    PrintJobHistoryService::RegisterProfilePrefs(test_prefs_.registry());
  }

  void TearDown() override {}

 protected:
  void SavePrintJob(const PrintJobInfo& print_job_info) {
    base::RunLoop run_loop;
    print_job_database_->SavePrintJob(
        print_job_info,
        base::BindOnce(&PrintJobHistoryCleanerTest::OnPrintJobSaved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnPrintJobSaved(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  std::vector<PrintJobInfo> GetPrintJobs() {
    base::RunLoop run_loop;
    print_job_database_->GetPrintJobs(
        base::BindOnce(&PrintJobHistoryCleanerTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

  void OnPrintJobsRetrieved(
      base::RepeatingClosure run_loop_closure,
      bool success,
      std::unique_ptr<std::vector<PrintJobInfo>> entries) {
    EXPECT_TRUE(success);
    entries_ = *entries;
    run_loop_closure.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<PrintJobDatabase> print_job_database_;
  std::unique_ptr<PrintJobHistoryCleaner> print_job_history_cleaner_;

 private:
  std::vector<PrintJobInfo> entries_;
};

TEST_F(PrintJobHistoryCleanerTest, CleanExpiredPrintJobs) {
  // Set expiration period to 1 day.
  test_prefs_.SetInteger(prefs::kPrintJobHistoryExpirationPeriod, 1);
  print_job_database_->Initialize(base::DoNothing());

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(300));
  PrintJobInfo print_job_info1 =
      ConstructPrintJobInfo(kId1, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info1);

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  PrintJobInfo print_job_info2 =
      ConstructPrintJobInfo(kId2, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info2);

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));
  print_job_history_cleaner_->Start();
  task_environment_.RunUntilIdle();

  std::vector<PrintJobInfo> entries = GetPrintJobs();
  // Start() call should clear the first entry which is expected to expire.
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(kId2, entries[0].id());

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  task_environment_.RunUntilIdle();
  entries = GetPrintJobs();
  // The second entry is expected to be removed after cleaner ran again.
  EXPECT_TRUE(entries.empty());
}

TEST_F(PrintJobHistoryCleanerTest, StorePrintJobHistoryIndefinite) {
  // Set expiration period policy to store history indefinitely.
  test_prefs_.SetInteger(prefs::kPrintJobHistoryExpirationPeriod, -1);
  print_job_database_->Initialize(base::DoNothing());

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(300));
  PrintJobInfo print_job_info1 =
      ConstructPrintJobInfo(kId1, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info1);

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(300));

  print_job_history_cleaner_->Start();
  task_environment_.RunUntilIdle();

  std::vector<PrintJobInfo> entries = GetPrintJobs();
  // Start() call shouldn't clear anything as according to pref we store history
  // indefinitely.
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());
}

}  // namespace chromeos
