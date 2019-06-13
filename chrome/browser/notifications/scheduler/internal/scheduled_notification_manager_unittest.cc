// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/collection_store.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;
using Entries = std::vector<std::unique_ptr<notifications::NotificationEntry>>;

namespace notifications {
namespace {

const char kGuid[] = "test_guid_1234";
const char kTitle[] = "test_title";

NotificationEntry CreateNotificationEntry() {
  return NotificationEntry(SchedulerClientType::kUnknown, base::GenerateGUID());
}

NotificationEntry CreateNotificationEntry(SchedulerClientType type) {
  return NotificationEntry(type, base::GenerateGUID());
}

class MockDelegate : public ScheduledNotificationManager::Delegate {
 public:
  MockDelegate() = default;
  MOCK_METHOD1(DisplayNotification, void(std::unique_ptr<NotificationEntry>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class MockNotificationStore : public CollectionStore<NotificationEntry> {
 public:
  MockNotificationStore() {}

  MOCK_METHOD1(InitAndLoad,
               void(CollectionStore<NotificationEntry>::LoadCallback));
  MOCK_METHOD3(Add,
               void(const std::string&,
                    const NotificationEntry&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD3(Update,
               void(const std::string&,
                    const NotificationEntry&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD2(Delete,
               void(const std::string&, base::OnceCallback<void(bool)>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNotificationStore);
};

class ScheduledNotificationManagerTest : public testing::Test {
 public:
  ScheduledNotificationManagerTest() : store_(nullptr) {}
  ~ScheduledNotificationManagerTest() override = default;

  void SetUp() override {
    delegate_ = std::make_unique<MockDelegate>();
    auto store = std::make_unique<MockNotificationStore>();
    store_ = store.get();
    manager_ = ScheduledNotificationManager::Create(std::move(store));
  }

 protected:
  ScheduledNotificationManager* manager() { return manager_.get(); }
  MockNotificationStore* store() { return store_; }
  MockDelegate* delegate() { return delegate_.get(); }

  // Initializes the manager with predefined data in the store.
  void InitWithData(std::vector<NotificationEntry> data) {
    Entries entries;
    for (auto it = data.begin(); it != data.end(); ++it) {
      auto entry_ptr = std::make_unique<NotificationEntry>(
          SchedulerClientType::kUnknown, it->guid);
      *(entry_ptr.get()) = *it;
      entries.emplace_back(std::move(entry_ptr));
    }

    // Initialize the store and call the callback.
    EXPECT_CALL(*store(), InitAndLoad(_))
        .WillOnce(
            Invoke([&entries](base::OnceCallback<void(bool, Entries)> cb) {
              std::move(cb).Run(true, std::move(entries));
            }));
    base::RunLoop loop;
    manager()->Init(delegate(),
                    base::BindOnce(
                        [](base::RepeatingClosure closure, bool success) {
                          EXPECT_TRUE(success);
                          std::move(closure).Run();
                        },
                        loop.QuitClosure()));
    loop.Run();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<MockDelegate> delegate_;
  MockNotificationStore* store_;
  std::unique_ptr<ScheduledNotificationManager> manager_;
  DISALLOW_COPY_AND_ASSIGN(ScheduledNotificationManagerTest);
};

// Verify that error is received when initialization failed.
TEST_F(ScheduledNotificationManagerTest, InitFailed) {
  EXPECT_CALL(*store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(bool, Entries)> cb) {
        std::move(cb).Run(false, Entries());
      }));

  base::RunLoop loop;
  manager()->Init(delegate(),
                  base::BindOnce(
                      [](base::RepeatingClosure closure, bool success) {
                        // Expected to receive error.
                        EXPECT_FALSE(success);
                        std::move(closure).Run();
                      },
                      loop.QuitClosure()));
  loop.Run();
}

// Test to schedule a notification.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotification) {
  InitWithData(std::vector<NotificationEntry>());
  NotificationData notification_data;
  notification_data.title = kTitle;
  ScheduleParams schedule_params;
  schedule_params.priority = ScheduleParams::Priority::kHigh;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kUnknown, notification_data, schedule_params);
  std::string guid = params->guid;
  EXPECT_FALSE(guid.empty());

  // Verify call contract.
  EXPECT_CALL(*store(), Add(guid, _, _));
  manager()->ScheduleNotification(std::move(params));

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  const NotificationEntry* entry = *(notifications.begin()->second.begin());
  EXPECT_EQ(entry->guid, guid);
  EXPECT_NE(entry->create_time, base::Time());

  // TODO(xingliu): change these to compare with operator==.
  EXPECT_EQ(entry->notification_data.title, kTitle);
  EXPECT_EQ(entry->schedule_params.priority, ScheduleParams::Priority::kHigh);
}

// Test to schedule a notification without guid, we will auto generated one.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotificationEmptyGuid) {
  InitWithData(std::vector<NotificationEntry>());
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kUnknown, NotificationData(), ScheduleParams());

  // Verify call contract.
  EXPECT_CALL(*store(), Add(_, _, _));
  manager()->ScheduleNotification(std::move(params));

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  const NotificationEntry* entry = *(notifications.begin()->second.begin());
  EXPECT_NE(entry->guid, std::string());
  EXPECT_NE(entry->create_time, base::Time());
}

// TODO(xingliu): change this to compare with operator==.
MATCHER_P(NotificationEntryIs, expected, "") {
  return arg->guid == expected.guid;
}

// Test to display a notification.
TEST_F(ScheduledNotificationManagerTest, DisplayNotification) {
  auto entry = CreateNotificationEntry();
  entry.guid = kGuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  // Verify delegate and dependency call contract.
  EXPECT_CALL(*store(), Delete(kGuid, _));
  EXPECT_CALL(*delegate(), DisplayNotification(NotificationEntryIs(entry)));
  manager()->DisplayNotification(kGuid);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_TRUE(notifications.empty());
}

// Verify GetAllNotifications API, the notification should be sorted based on
// creation timestamp.
TEST_F(ScheduledNotificationManagerTest, GetAllNotifications) {
  // Ordering: entry1.create_time < entry0.create_time < entry2.create_time.
  auto now = base::Time::Now();
  auto entry0 = CreateNotificationEntry();
  entry0.create_time = now;
  auto entry1 = CreateNotificationEntry();
  entry1.create_time = now - base::TimeDelta::FromMinutes(1);
  auto entry2 = CreateNotificationEntry();
  entry2.create_time = now + base::TimeDelta::FromMinutes(1);

  InitWithData(std::vector<NotificationEntry>({entry0, entry1, entry2}));
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  EXPECT_EQ(notifications.begin()->second.size(), 3u);

  // Entries returned by GetAllNotifications() should be sorted by creation
  // timestamp.
  const NotificationEntry* output0 = notifications.begin()->second.front();
  const NotificationEntry* output2 = notifications.begin()->second.back();
  EXPECT_EQ(output0->create_time, entry1.create_time);
  EXPECT_EQ(output2->create_time, entry2.create_time);
}

// Verify DeleteNotifications API, all notifications with given
// SchedulerClientType should be deleted.
TEST_F(ScheduledNotificationManagerTest, DeleteNotifications) {
  // Type1: entry0
  // Type2: entry1, entry2
  // Type3: entry3
  auto entry0 = CreateNotificationEntry(SchedulerClientType::kTest1);
  auto entry1 = CreateNotificationEntry(SchedulerClientType::kTest2);
  auto entry2 = CreateNotificationEntry(SchedulerClientType::kTest2);
  auto entry3 = CreateNotificationEntry(SchedulerClientType::kTest3);

  InitWithData(
      std::vector<NotificationEntry>({entry0, entry1, entry2, entry3}));
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 3u);

  EXPECT_CALL(*store(), Delete(_, _)).Times(2).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest2);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);

  // Ensure deleting non-existing key will not crash, and store will not call
  // Delete.
  EXPECT_CALL(*store(), Delete(_, _)).Times(0).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest2);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);

  EXPECT_CALL(*store(), Delete(_, _)).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest1);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);

  EXPECT_CALL(*store(), Delete(_, _)).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest3);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 0u);
}
}  // namespace
}  // namespace notifications
