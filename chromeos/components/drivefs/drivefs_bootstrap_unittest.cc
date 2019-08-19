// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_bootstrap.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/components/drivefs/pending_connection_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

class MockDriveFs : public mojom::DriveFsInterceptorForTesting {
 public:
  DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }
};

class MockDriveFsDelegate : public mojom::DriveFsDelegateInterceptorForTesting {
 public:
  DriveFsDelegate* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }
};

class DriveFsBootstrapListenerForTest : public DriveFsBootstrapListener {
 public:
  DriveFsBootstrapListenerForTest(
      mojom::DriveFsBootstrapPtrInfo available_bootstrap)
      : available_bootstrap_(std::move(available_bootstrap)) {}

  mojom::DriveFsBootstrapPtr bootstrap() override {
    return mojo::MakeProxy(std::move(available_bootstrap_));
  }

  void SendInvitationOverPipe(base::ScopedFD) override {}

 private:
  mojom::DriveFsBootstrapPtrInfo available_bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsBootstrapListenerForTest);
};

class DriveFsBootstrapTest : public testing::Test,
                             public mojom::DriveFsBootstrap {
 public:
  DriveFsBootstrapTest() : bootstrap_binding_(this), binding_(&mock_drivefs_) {}

 protected:
  MOCK_CONST_METHOD0(OnDisconnect, void());
  MOCK_CONST_METHOD0(OnInit, void());

  void Init(mojom::DriveFsConfigurationPtr config,
            mojom::DriveFsRequest drive_fs_request,
            mojom::DriveFsDelegatePtr delegate) override {
    binding_.Bind(std::move(drive_fs_request));
    mojo::FuseInterface(std::move(pending_delegate_request_),
                        delegate.PassInterface());
    OnInit();
  }

  std::unique_ptr<DriveFsBootstrapListener> CreateListener() {
    mojom::DriveFsBootstrapPtrInfo pending_bootstrap;
    bootstrap_binding_.Bind(mojo::MakeRequest(&pending_bootstrap));
    pending_delegate_request_ = mojo::MakeRequest(&delegate_ptr_);
    return std::make_unique<DriveFsBootstrapListenerForTest>(
        std::move(pending_bootstrap));
  }

  base::UnguessableToken ListenForConnection() {
    connection_ = DriveFsConnection::Create(CreateListener(),
                                            mojom::DriveFsConfiguration::New());
    return connection_->Connect(
        &mock_delegate_, base::BindOnce(&DriveFsBootstrapTest::OnDisconnect,
                                        base::Unretained(this)));
  }

  void WaitForConnection(const base::UnguessableToken& token) {
    ASSERT_TRUE(
        PendingConnectionManager::Get().OpenIpcChannel(token.ToString(), {}));
    base::RunLoop run_loop;
    bootstrap_binding_.set_connection_error_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  MockDriveFs mock_drivefs_;
  MockDriveFsDelegate mock_delegate_;

  mojo::Binding<mojom::DriveFsBootstrap> bootstrap_binding_;
  mojo::Binding<mojom::DriveFs> binding_;
  std::unique_ptr<DriveFsConnection> connection_;
  mojom::DriveFsDelegatePtr delegate_ptr_;
  mojom::DriveFsDelegateRequest pending_delegate_request_;
  std::string email_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsBootstrapTest);
};

}  // namespace

TEST_F(DriveFsBootstrapTest, Listen_Connect_Disconnect) {
  auto token = ListenForConnection();
  EXPECT_CALL(*this, OnInit());
  WaitForConnection(token);
  EXPECT_CALL(*this, OnDisconnect());
  binding_.Close();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(
      PendingConnectionManager::Get().OpenIpcChannel(token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_Connect_DisconnectDelegate) {
  auto token = ListenForConnection();
  EXPECT_CALL(*this, OnInit());
  WaitForConnection(token);
  EXPECT_CALL(*this, OnDisconnect());
  delegate_ptr_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(
      PendingConnectionManager::Get().OpenIpcChannel(token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_Connect_Destroy) {
  auto token = ListenForConnection();
  EXPECT_CALL(*this, OnInit());
  WaitForConnection(token);
  EXPECT_CALL(*this, OnDisconnect()).Times(0);
  connection_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(
      PendingConnectionManager::Get().OpenIpcChannel(token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_Destroy) {
  EXPECT_CALL(*this, OnDisconnect()).Times(0);
  auto token = ListenForConnection();
  connection_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(
      PendingConnectionManager::Get().OpenIpcChannel(token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_DisconnectDelegate) {
  EXPECT_CALL(*this, OnDisconnect()).Times(0);
  ListenForConnection();
  delegate_ptr_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace drivefs
