// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/auto_resumption_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/network/network_status_listener_impl.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/task/mock_task_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using network::mojom::ConnectionType;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRefOfCopy;

namespace download {

class AutoResumptionHandlerTest : public testing::Test {
 public:
  AutoResumptionHandlerTest()
      : task_runner_(new base::TestMockTimeTaskRunner), handle_(task_runner_) {}

  ~AutoResumptionHandlerTest() override = default;

 protected:
  void SetUp() override {
    auto network_listener = std::make_unique<NetworkStatusListenerImpl>(
        network::TestNetworkConnectionTracker::GetInstance());
    auto task_manager = std::make_unique<download::test::MockTaskManager>();
    task_manager_ = task_manager.get();

    auto config = std::make_unique<AutoResumptionHandler::Config>();
    config->auto_resumption_size_limit = 100;
    config->is_auto_resumption_enabled_in_native = true;

    auto_resumption_handler_ = std::make_unique<AutoResumptionHandler>(
        std::move(network_listener), std::move(task_manager),
        std::move(config));

    std::vector<DownloadItem*> empty_list;
    auto_resumption_handler_->SetResumableDownloads(empty_list);
    task_runner_->FastForwardUntilNoTasksRemain();
  }

  void TearDown() override {}

  void SetDownloadState(MockDownloadItem* download,
                        DownloadItem::DownloadState state,
                        bool paused,
                        bool metered,
                        bool has_target_file_path = true) {
    ON_CALL(*download, GetGuid())
        .WillByDefault(ReturnRefOfCopy(base::GenerateGUID()));
    ON_CALL(*download, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("http://example.com/foo")));
    ON_CALL(*download, GetState()).WillByDefault(Return(state));
    ON_CALL(*download, IsPaused()).WillByDefault(Return(paused));
    ON_CALL(*download, AllowMetered()).WillByDefault(Return(metered));
    ON_CALL(*download, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(
            has_target_file_path ? base::FilePath(FILE_PATH_LITERAL("a.txt"))
                                 : base::FilePath()));
    auto last_reason =
        state == DownloadItem::INTERRUPTED
            ? download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED
            : download::DOWNLOAD_INTERRUPT_REASON_NONE;
    ON_CALL(*download, GetLastReason()).WillByDefault(Return(last_reason));
  }

  void SetNetworkConnectionType(ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;
  download::test::MockTaskManager* task_manager_;
  std::unique_ptr<AutoResumptionHandler> auto_resumption_handler_;

  DISALLOW_COPY_AND_ASSIGN(AutoResumptionHandlerTest);
};

TEST_F(AutoResumptionHandlerTest, ScheduleTaskCalledOnDownloadStart) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, TaskFinishedCalledOnDownloadCompletion) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Complete the download.
  EXPECT_CALL(*task_manager_, NotifyTaskFinished(_, _)).Times(1);
  EXPECT_CALL(*task_manager_, UnscheduleTask(_)).Times(1);
  SetDownloadState(item.get(), DownloadItem::COMPLETE, false, false);
  auto_resumption_handler_->OnDownloadUpdated(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, TaskFinishedCalledOnDownloadRemoved) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Remove the download.
  EXPECT_CALL(*task_manager_, NotifyTaskFinished(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::COMPLETE, false, false);
  auto_resumption_handler_->OnDownloadRemoved(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, MultipleDownloads) {
  // Start two downloads.
  auto item1 = std::make_unique<NiceMock<MockDownloadItem>>();
  auto item2 = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item1.get(), DownloadItem::INTERRUPTED, false, false);
  SetDownloadState(item2.get(), DownloadItem::INTERRUPTED, false, false);

  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  auto_resumption_handler_->OnDownloadStarted(item1.get());
  auto_resumption_handler_->OnDownloadStarted(item2.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Finish item1. The task should still be running.
  EXPECT_CALL(*task_manager_, UnscheduleTask(_)).Times(0);
  EXPECT_CALL(*task_manager_, NotifyTaskFinished(_, _)).Times(0);
  SetDownloadState(item1.get(), DownloadItem::CANCELLED, false, false);
  auto_resumption_handler_->OnDownloadUpdated(item1.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Finish item2. The task should now complete.
  EXPECT_CALL(*task_manager_, UnscheduleTask(_)).Times(1);
  EXPECT_CALL(*task_manager_, NotifyTaskFinished(_, _)).Times(1);
  SetDownloadState(item2.get(), DownloadItem::COMPLETE, false, false);
  auto_resumption_handler_->OnDownloadUpdated(item2.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, DownloadResumesCorrectlyOnNetworkChange) {
  // Create two downloads: item1 (unmetered), item2 (metered).
  auto item1 = std::make_unique<NiceMock<MockDownloadItem>>();
  auto item2 = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item1.get(), DownloadItem::INTERRUPTED, false, false);
  SetDownloadState(item2.get(), DownloadItem::INTERRUPTED, false, true);

  auto_resumption_handler_->OnDownloadStarted(item1.get());
  auto_resumption_handler_->OnDownloadStarted(item2.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Start with disconnected network.
  SetNetworkConnectionType(ConnectionType::CONNECTION_NONE);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Connect to Wifi.
  EXPECT_CALL(*item1.get(), Resume(_)).Times(1);
  EXPECT_CALL(*item2.get(), Resume(_)).Times(1);
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Disconnect network again.
  EXPECT_CALL(*item1.get(), Resume(_)).Times(0);
  EXPECT_CALL(*item2.get(), Resume(_)).Times(0);
  SetNetworkConnectionType(ConnectionType::CONNECTION_NONE);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Change network to metered.
  EXPECT_CALL(*item1.get(), Resume(_)).Times(0);
  EXPECT_CALL(*item2.get(), Resume(_)).Times(1);
  SetNetworkConnectionType(ConnectionType::CONNECTION_3G);
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, PausedDownloadsAreNotAutoResumed) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, true, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());

  SetNetworkConnectionType(ConnectionType::CONNECTION_NONE);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Connect to Wifi.
  EXPECT_CALL(*item.get(), Resume(_)).Times(0);
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest,
       OnStartScheduledTaskWillResumeAllPendingDownloads) {
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item.get(), DownloadItem::INTERRUPTED, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Start the task. It should resume all downloads.
  EXPECT_CALL(*item.get(), Resume(_)).Times(1);
  TaskFinishedCallback callback;
  auto_resumption_handler_->OnStartScheduledTask(std::move(callback));
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, DownloadWithoutTargetPathNotAutoResumed) {
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item.get(), DownloadItem::INTERRUPTED, false, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*item.get(), Resume(_)).Times(0);
  TaskFinishedCallback callback;
  auto_resumption_handler_->OnStartScheduledTask(std::move(callback));
  task_runner_->FastForwardUntilNoTasksRemain();
}
}  // namespace download
