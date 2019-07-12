// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/collection_store.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
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

class MockIconStore : public IconStore {
 public:
  MockIconStore() {}

  MOCK_METHOD1(Init, void(IconStore::InitCallback));
  MOCK_METHOD2(Load, void(const std::string&, IconStore::LoadCallback));
  MOCK_METHOD3(Add,
               void(const std::string&,
                    std::unique_ptr<IconEntry>,
                    IconStore::UpdateCallback));
  MOCK_METHOD2(Delete, void(const std::string&, IconStore::UpdateCallback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIconStore);
};

class ScheduledNotificationManagerTest : public testing::Test {
 public:
  ScheduledNotificationManagerTest()
      : notification_store_(nullptr), icon_store_(nullptr) {}
  ~ScheduledNotificationManagerTest() override = default;

  void SetUp() override {
    delegate_ = std::make_unique<MockDelegate>();
    auto notification_store = std::make_unique<MockNotificationStore>();
    auto icon_store = std::make_unique<MockIconStore>();
    notification_store_ = notification_store.get();
    icon_store_ = icon_store.get();
    config_.notification_expiration = base::TimeDelta::FromDays(1);
    manager_ = ScheduledNotificationManager::Create(
        std::move(notification_store), std::move(icon_store),
        {SchedulerClientType::kTest1, SchedulerClientType::kTest2,
         SchedulerClientType::kTest3},
        config_);
  }

 protected:
  ScheduledNotificationManager* manager() { return manager_.get(); }
  MockNotificationStore* notification_store() { return notification_store_; }
  MockIconStore* icon_store() { return icon_store_; }
  MockDelegate* delegate() { return delegate_.get(); }
  const SchedulerConfig& config() const { return config_; }
  // Initializes the manager with predefined data in the store.
  void InitWithData(std::vector<NotificationEntry> data) {
    Entries entries;
    for (auto it = data.begin(); it != data.end(); ++it) {
      auto entry_ptr = std::make_unique<NotificationEntry>(it->type, it->guid);
      *(entry_ptr.get()) = *it;
      entries.emplace_back(std::move(entry_ptr));
    }

    // Initialize the store and call the callback.
    EXPECT_CALL(*notification_store(), InitAndLoad(_))
        .WillOnce(
            Invoke([&entries](base::OnceCallback<void(bool, Entries)> cb) {
              std::move(cb).Run(true, std::move(entries));
            }));
    EXPECT_CALL(*icon_store(), Init(_))
        .WillOnce(Invoke([](base::OnceCallback<void(bool)> cb) {
          std::move(cb).Run(true);
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
  MockNotificationStore* notification_store_;
  MockIconStore* icon_store_;
  std::vector<SchedulerClientType> clients_;
  std::unique_ptr<ScheduledNotificationManager> manager_;
  SchedulerConfig config_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledNotificationManagerTest);
};

// Verify that error is received when initialization failed.
TEST_F(ScheduledNotificationManagerTest, InitFailed) {
  EXPECT_CALL(*notification_store(), InitAndLoad(_))
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
  notification_data.title = base::UTF8ToUTF16(kTitle);
  ScheduleParams schedule_params;
  schedule_params.priority = ScheduleParams::Priority::kHigh;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, notification_data, schedule_params);
  std::string guid = params->guid;
  EXPECT_FALSE(guid.empty());

  // Verify call contract.
  EXPECT_CALL(*notification_store(), Add(guid, _, _));
  manager()->ScheduleNotification(std::move(params));

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  const NotificationEntry* entry = *(notifications.begin()->second.begin());
  EXPECT_EQ(entry->guid, guid);
  EXPECT_NE(entry->create_time, base::Time());

  // TODO(xingliu): change these to compare with operator==.
  EXPECT_EQ(base::UTF16ToUTF8(entry->notification_data.title), kTitle);
  EXPECT_EQ(entry->schedule_params.priority, ScheduleParams::Priority::kHigh);
}

// Test to schedule a notification without guid, we will auto generated one.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotificationEmptyGuid) {
  InitWithData(std::vector<NotificationEntry>());
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, NotificationData(), ScheduleParams());

  // Verify call contract.
  EXPECT_CALL(*notification_store(), Add(_, _, _));
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
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry.guid = kGuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  // Verify delegate and dependency call contract.
  EXPECT_CALL(*notification_store(), Delete(kGuid, _));
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
  auto entry0 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry0.create_time = now;
  auto entry1 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry1.create_time = now - base::TimeDelta::FromMinutes(1);
  auto entry2 = CreateNotificationEntry(SchedulerClientType::kTest1);
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

// Verifies GetNotifications() can return the correct values.
TEST_F(ScheduledNotificationManagerTest, GetNotifications) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  InitWithData(std::vector<NotificationEntry>({entry}));
  std::vector<const NotificationEntry*> entries;
  manager()->GetNotifications(SchedulerClientType::kTest2, &entries);
  EXPECT_TRUE(entries.empty());

  manager()->GetNotifications(SchedulerClientType::kTest1, &entries);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(*entries.back(), entry);

  manager()->GetNotifications(SchedulerClientType::kTest2, &entries);
  EXPECT_TRUE(entries.empty());
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

  EXPECT_CALL(*notification_store(), Delete(_, _))
      .Times(2)
      .RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest2);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);

  // Ensure deleting non-existing key will not crash, and store will not call
  // Delete.
  EXPECT_CALL(*notification_store(), Delete(_, _))
      .Times(0)
      .RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest2);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);

  EXPECT_CALL(*notification_store(), Delete(_, _)).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest1);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);

  EXPECT_CALL(*notification_store(), Delete(_, _)).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest3);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 0u);
}

TEST_F(ScheduledNotificationManagerTest, PruneExpiredNotifications) {
  // Type1: entry0
  // Type2: entry1, entry2(expired), entry3
  // Type3: entry4(expired), entry5(expired)
  auto now = base::Time::Now();
  auto entry0 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry0.create_time = now - base::TimeDelta::FromHours(12);
  auto entry1 = CreateNotificationEntry(SchedulerClientType::kTest2);
  entry1.create_time = now - base::TimeDelta::FromHours(14);
  auto entry2 = CreateNotificationEntry(SchedulerClientType::kTest2);
  entry2.create_time = now - base::TimeDelta::FromHours(24);
  auto entry3 = CreateNotificationEntry(SchedulerClientType::kTest2);
  entry3.create_time = now - base::TimeDelta::FromHours(23);
  auto entry4 = CreateNotificationEntry(SchedulerClientType::kTest3);
  entry4.create_time = now - base::TimeDelta::FromHours(25);
  auto entry5 = CreateNotificationEntry(SchedulerClientType::kTest3);
  entry5.create_time =
      now - base::TimeDelta::FromDays(1) - base::TimeDelta::FromMicroseconds(1);

  EXPECT_CALL(*notification_store(), Delete(_, _))
      .Times(3)
      .RetiresOnSaturation();
  InitWithData(std::vector<NotificationEntry>(
      {entry0, entry1, entry2, entry3, entry4, entry5}));
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);
  EXPECT_EQ(notifications[SchedulerClientType::kTest1].size(), 1u);
  EXPECT_EQ(notifications[SchedulerClientType::kTest2].size(), 2u);
}

}  // namespace
}  // namespace notifications
