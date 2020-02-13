// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/internal/update_notification_service_impl.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/updates/test/mock_update_notification_service_bridge.h"
#include "chrome/browser/updates/update_notification_config.h"

namespace updates {
namespace {

class UpdateNotificationServiceImplTest : public testing::Test {
 public:
  UpdateNotificationServiceImplTest() : bridge_(nullptr), config_(nullptr) {}
  ~UpdateNotificationServiceImplTest() override = default;

  void SetUp() override {
    scheduler_ = std::make_unique<
        notifications::test::MockNotificationScheduleService>();
    auto bridge = std::make_unique<test::MockUpdateNotificationServiceBridge>();
    bridge_ = bridge.get();
    auto config = UpdateNotificationConfig::Create();
    config_ = config.get();

    service_ = std::make_unique<updates::UpdateNotificationServiceImpl>(
        scheduler_.get(), std::move(config), std::move(bridge));
  }

 protected:
  notifications::test::MockNotificationScheduleService* scheduler() {
    return scheduler_.get();
  }

  test::MockUpdateNotificationServiceBridge* bridge() { return bridge_; }

  UpdateNotificationService* service() { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  test::MockUpdateNotificationServiceBridge* bridge_;
  std::unique_ptr<notifications::test::MockNotificationScheduleService>
      scheduler_;
  UpdateNotificationConfig* config_;

  std::unique_ptr<UpdateNotificationService> service_;
  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationServiceImplTest);
};

}  // namespace
}  // namespace updates
