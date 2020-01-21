// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/smbfs/smbfs_host.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace smbfs {
namespace {

constexpr base::FilePath::CharType kMountPath[] = FILE_PATH_LITERAL("/foo/bar");

class MockDelegate : public SmbFsHost::Delegate {
 public:
  MOCK_METHOD(void, OnDisconnected, (), (override));
};

class SmbFsHostTest : public testing::Test {
 protected:
  void SetUp() override {
    smbfs_pending_receiver_ = smbfs_remote_.BindNewPipeAndPassReceiver();
    delegate_pending_receiver_ = delegate_remote_.BindNewPipeAndPassReceiver();
  }

  base::test::TaskEnvironment task_environment_;

  MockDelegate mock_delegate_;
  chromeos::disks::MockDiskMountManager mock_disk_mount_manager_;

  mojo::Remote<mojom::SmbFs> smbfs_remote_;
  mojo::PendingReceiver<mojom::SmbFs> smbfs_pending_receiver_;
  mojo::Remote<mojom::SmbFsDelegate> delegate_remote_;
  mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_pending_receiver_;
};

TEST_F(SmbFsHostTest, DisconnectDelegate) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnDisconnected())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(chromeos::MOUNT_ERROR_NONE));

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<chromeos::disks::MountPoint>(base::FilePath(kMountPath),
                                                    &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  delegate_remote_.reset();

  run_loop.Run();
}

TEST_F(SmbFsHostTest, DisconnectSmbFs) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnDisconnected())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(chromeos::MOUNT_ERROR_NONE));

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<chromeos::disks::MountPoint>(base::FilePath(kMountPath),
                                                    &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  smbfs_pending_receiver_.reset();

  run_loop.Run();
}

TEST_F(SmbFsHostTest, UnmountOnDestruction) {
  EXPECT_CALL(mock_delegate_, OnDisconnected()).Times(0);
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(chromeos::MOUNT_ERROR_NONE));

  base::RunLoop run_loop;
  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<chromeos::disks::MountPoint>(base::FilePath(kMountPath),
                                                    &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  run_loop.RunUntilIdle();
  host.reset();
}

}  // namespace
}  // namespace smbfs
