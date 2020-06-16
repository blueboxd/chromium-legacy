// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_out_of_process.h"

#import <Foundation/Foundation.h>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_policy.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#include "chrome/updater/mac/scoped_xpc_service_mock.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace updater {

class MacUpdateServiceOutOfProcessTest : public ::testing::Test {
 protected:
  void SetUp() override;

  // Create an UpdateServiceOutOfProcess and store in service_. Must be
  // called only on the task_environment_ sequence. SetUp() posts it.
  void InitializeService();

  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedXPCServiceMock mock_driver_ { @protocol (CRUUpdateChecking) };
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<UpdateServiceOutOfProcess> service_;
};  // class MacUpdateOutOfProcessTest

void MacUpdateServiceOutOfProcessTest::SetUp() {
  run_loop_ = std::make_unique<base::RunLoop>();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        service_ = base::MakeRefCounted<UpdateServiceOutOfProcess>(
            UpdateService::Scope::kUser);
      }));
}

TEST_F(MacUpdateServiceOutOfProcessTest, NoProductsUpdateAll) {
  ScopedXPCServiceMock::ConnectionMockRecord* conn_rec =
      mock_driver_.PrepareNewMockConnection();
  ScopedXPCServiceMock::RemoteObjectMockRecord* mock_rec =
      conn_rec->PrepareNewMockRemoteObject();
  id<CRUUpdateChecking> mock_remote_object = mock_rec->mock_object.get();

  OCMockBlockCapturer<void (^)(UpdateService::Result)> reply_block_capturer;
  // Create a pointer that can be copied into the .andDo block to refer to the
  // capturer so we can invoke the captured block.
  auto* reply_block_capturer_ptr = &reply_block_capturer;
  OCMExpect([mock_remote_object
                checkForUpdatesWithUpdateState:[OCMArg isNotNil]
                                         reply:reply_block_capturer.Capture()])
      .andDo(^(NSInvocation*) {
        reply_block_capturer_ptr->Get()[0].get()(
            UpdateService::Result::kAppNotFound);
      });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        service_->UpdateAll(
            base::BindRepeating(&ExpectNoCalls<UpdateService::UpdateState>,
                                "no state updates expected"),
            base::BindLambdaForTesting(
                [this](UpdateService::Result actual_result) {
                  EXPECT_EQ(UpdateService::Result::kAppNotFound, actual_result);
                  run_loop_->QuitWhenIdle();
                }));
      }));

  run_loop_->Run();

  EXPECT_EQ(reply_block_capturer.Get().size(), 1UL);
  EXPECT_EQ(conn_rec->VendedObjectsCount(), 1UL);
  EXPECT_EQ(mock_driver_.VendedConnectionsCount(), 1UL);
  service_.reset();  // Drop service reference - should invalidate connection
  mock_driver_.VerifyAll();
}

}  // namespace updater
