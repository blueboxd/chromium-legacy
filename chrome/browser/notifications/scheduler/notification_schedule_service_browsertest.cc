// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_impl.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace notifications {
namespace {

const base::FilePath::CharType kTestDir[] =
    FILE_PATH_LITERAL("NotificationScheduleServiceTest");

class TestClient : public NotificationSchedulerClient {
 public:
  TestClient() {}
  ~TestClient() override = default;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override {
    std::move(callback).Run(std::move(notification_data));
  }

  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override {
    DCHECK(success);
  }

  void OnUserAction(const UserActionData& action_data) override {}

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

class TestBackgroundTaskScheduler : public NotificationBackgroundTaskScheduler {
 public:
  TestBackgroundTaskScheduler() = default;
  ~TestBackgroundTaskScheduler() override = default;

  // Waits until a background task has been updated.
  void WaitForTaskUpdated() {
    DCHECK(!run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  base::TimeDelta window_start() { return window_start_; }

 private:
  void QuitRunLoopIfNeeded() {
    if (run_loop_ && run_loop_->running()) {
      run_loop_->Quit();
    }
  }

  // NotificationBackgroundTaskScheduler implementation.
  void Schedule(notifications::SchedulerTaskTime scheduler_task_time,
                base::TimeDelta window_start,
                base::TimeDelta window_end) override {
    QuitRunLoopIfNeeded();
  }

  void Cancel() override { QuitRunLoopIfNeeded(); }

  base::TimeDelta window_start_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundTaskScheduler);
};

// Browser test for notification scheduling system. Uses real database
// instances. Mainly to cover service initialization flow in chrome layer.
class NotificationScheduleServiceTest : public InProcessBrowserTest {
 public:
  NotificationScheduleServiceTest() : task_scheduler_(nullptr) {
    scoped_feature_list_.InitWithFeatures(
        {features::kNotificationScheduleService}, {});
  }

  ~NotificationScheduleServiceTest() override {}

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    ASSERT_TRUE(tmp_dir_.Delete());
  }

  // Initializes |service_|. Injects database test data before this call.
  void Init() {
    auto* profile = browser()->profile();
    auto client = std::make_unique<TestClient>();
    auto client_registrar =
        std::make_unique<NotificationSchedulerClientRegistrar>();
    client_registrar->RegisterClient(SchedulerClientType::kTest1,
                                     std::move(client));
    auto display_agent = notifications::DisplayAgent::Create();
    auto background_task_scheduler =
        std::make_unique<TestBackgroundTaskScheduler>();
    task_scheduler_ = background_task_scheduler.get();
    auto* db_provider =
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetProtoDatabaseProvider();
    NotificationScheduleService* service =
        static_cast<NotificationScheduleService*>(
            CreateNotificationScheduleService(
                std::move(client_registrar),
                std::move(background_task_scheduler), std::move(display_agent),
                db_provider, tmp_dir_.GetPath().Append(kTestDir),
                profile->IsOffTheRecord()));
    service_ = std::unique_ptr<NotificationScheduleService>(service);
  }

  NotificationScheduleService* schedule_service() { return service_.get(); }

  TestBackgroundTaskScheduler* task_scheduler() {
    DCHECK(task_scheduler_);
    return task_scheduler_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir tmp_dir_;
  std::unique_ptr<NotificationScheduleService> service_;
  TestBackgroundTaskScheduler* task_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleServiceTest);
};

// Test to schedule a notification.
IN_PROC_BROWSER_TEST_F(NotificationScheduleServiceTest, ScheduleNotification) {
  Init();
  ScheduleParams schedule_params;
  schedule_params.deliver_time_start = base::Time::Now();
  schedule_params.deliver_time_end =
      base::Time::Now() + base::TimeDelta::FromMinutes(5);
  NotificationData data;
  data.title = base::UTF8ToUTF16("title");
  data.message = base::UTF8ToUTF16("message");
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kTest1, std::move(data),
      std::move(schedule_params));
  schedule_service()->Schedule(std::move(params));

  // A background task should be scheduled.
  task_scheduler()->WaitForTaskUpdated();
  EXPECT_LE(task_scheduler()->window_start(), base::TimeDelta::FromMinutes(5));
}

}  // namespace
}  // namespace notifications
