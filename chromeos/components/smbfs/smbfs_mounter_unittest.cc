// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/smbfs/smbfs_mounter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;
using testing::StartsWith;
using testing::WithArg;

namespace smbfs {
namespace {

constexpr char kMountUrlPrefix[] = "smbfs://";
constexpr char kSharePath[] = "smb://server/share";
constexpr char kMountDir[] = "bar";
constexpr base::FilePath::CharType kMountPath[] = FILE_PATH_LITERAL("/foo/bar");
constexpr int kChildInvitationFd = 42;

class MockDelegate : public SmbFsHost::Delegate {
 public:
  MOCK_METHOD(void, OnDisconnected, (), (override));
};

class TestSmbFsBootstrapImpl : public mojom::SmbFsBootstrap {
 public:
  MOCK_METHOD(void,
              MountShare,
              (mojom::MountOptionsPtr,
               mojo::PendingRemote<mojom::SmbFsDelegate>,
               MountShareCallback),
              (override));
};

class TestSmbFsImpl : public mojom::SmbFs {};

chromeos::disks::DiskMountManager::MountPointInfo MakeMountPointInfo(
    const std::string& source_path,
    const std::string& mount_path) {
  return chromeos::disks::DiskMountManager::MountPointInfo(
      source_path, mount_path, chromeos::MOUNT_TYPE_NETWORK_STORAGE,
      chromeos::disks::MOUNT_CONDITION_NONE);
}

class SmbFsMounterTest : public testing::Test {
 public:
  void PostMountEvent(const std::string& source_path,
                      const std::string& mount_path) {
    base::PostTask(
        FROM_HERE, {base::CurrentThread()},
        base::BindOnce(&chromeos::disks::MockDiskMountManager::NotifyMountEvent,
                       base::Unretained(&mock_disk_mount_manager_),
                       chromeos::disks::DiskMountManager::MOUNTING,
                       chromeos::MOUNT_ERROR_NONE,
                       MakeMountPointInfo(source_path, mount_path)));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  mojo::core::ScopedIPCSupport ipc_support_{
      task_environment_.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN};

  MockDelegate mock_delegate_;
  chromeos::disks::MockDiskMountManager mock_disk_mount_manager_;
};

TEST_F(SmbFsMounterTest, FilesystemMountTimeout) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kTimeout);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);
  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _))
      .Times(1);
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(_, _)).Times(0);

  base::TimeTicks start_time = task_environment_.NowTicks();
  mounter->Mount(callback);

  // TaskEnvironment will automatically advance mock time to the next posted
  // task, which is the mount timeout in this case.
  run_loop.Run();

  EXPECT_GE(task_environment_.NowTicks() - start_time,
            base::TimeDelta::FromSeconds(20));
}

TEST_F(SmbFsMounterTest, TimeoutAfterFilesystemMount) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kTimeout);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);

  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _))
      .WillOnce(WithArg<0>([this](const std::string& source_path) {
        PostMountEvent(source_path, kMountPath);
      }));
  // Destructing SmbFsMounter on failure will cause the mount point to be
  // unmounted.
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _)).Times(1);

  base::TimeTicks start_time = task_environment_.NowTicks();
  mounter->Mount(callback);

  // TaskEnvironment will automatically advance mock time to the next posted
  // task, which is the mount timeout in this case.
  run_loop.Run();

  EXPECT_GE(task_environment_.NowTicks() - start_time,
            base::TimeDelta::FromSeconds(20));
}

class SmbFsMounterE2eTest : public testing::Test {
 public:
  void PostMountEvent(const std::string& source_path,
                      const std::string& mount_path) {
    base::PostTask(
        FROM_HERE, {base::CurrentThread()},
        base::BindOnce(&chromeos::disks::MockDiskMountManager::NotifyMountEvent,
                       base::Unretained(&mock_disk_mount_manager_),
                       chromeos::disks::DiskMountManager::MOUNTING,
                       chromeos::MOUNT_ERROR_NONE,
                       MakeMountPointInfo(source_path, mount_path)));
  }

 protected:
  // This test performs actual IPC using sockets, and therefore cannot use
  // MOCK_TIME, which automatically advances time when the main loop is idle.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  mojo::core::ScopedIPCSupport ipc_support_{
      task_environment_.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN};

  MockDelegate mock_delegate_;
  chromeos::disks::MockDiskMountManager mock_disk_mount_manager_;
};

// Child process that emulates the behaviour of smbfs.
MULTIPROCESS_TEST_MAIN(SmbFsMain) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED);
  mojo::core::ScopedIPCSupport ipc_support(
      task_environment.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  mojo::IncomingInvitation invitation =
      mojo::IncomingInvitation::Accept(mojo::PlatformChannelEndpoint(
          mojo::PlatformHandle(base::ScopedFD(kChildInvitationFd))));

  TestSmbFsImpl mock_smbfs;
  mojo::Receiver<mojom::SmbFs> smbfs_receiver(&mock_smbfs);

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);

  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&smbfs_receiver, &run_loop, &delegate_remote](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        EXPECT_EQ(options->share_path, kSharePath);
        EXPECT_TRUE(options->username.empty());
        EXPECT_TRUE(options->workgroup.empty());
        EXPECT_TRUE(options->password.empty());
        EXPECT_FALSE(options->allow_ntlm);

        delegate_remote = std::move(delegate);
        mojo::PendingRemote<mojom::SmbFs> smbfs =
            smbfs_receiver.BindNewPipeAndPassRemote();
        // When the SmbFsHost in the parent is destroyed, this message pipe will
        // be closed and treat that as a signal to shut down.
        smbfs_receiver.set_disconnect_handler(run_loop.QuitClosure());

        std::move(callback).Run(mojom::MountError::kOk, std::move(smbfs));
      });

  bootstrap_receiver.Bind(mojo::PendingReceiver<mojom::SmbFsBootstrap>(
      invitation.ExtractMessagePipe("smbfs-bootstrap")));

  run_loop.Run();

  return 0;
}

TEST_F(SmbFsMounterE2eTest, MountSuccess) {
  mojo::PlatformChannel channel;

  base::LaunchOptions launch_options;
  base::ScopedFD child_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
  launch_options.fds_to_remap.push_back(
      std::make_pair(child_fd.get(), kChildInvitationFd));
  base::Process child_process = base::SpawnMultiProcessTestChild(
      "SmbFsMain", base::GetMultiProcessTestChildBaseCommandLine(),
      launch_options);
  ASSERT_TRUE(child_process.IsValid());
  // The child FD has been passed to the child process at this point.
  ignore_result(child_fd.release());

  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _))
      .WillOnce(WithArg<0>([this, &channel](const std::string& source_path) {
        // Emulates cros-disks mount success.
        PostMountEvent(source_path, kMountPath);

        // Emulates smbfs connecting to the org.chromium.SmbFs D-Bus service and
        // providing a Mojo connection endpoint.
        const std::string token =
            source_path.substr(sizeof(kMountUrlPrefix) - 1);
        base::PostTask(
            FROM_HERE, {base::CurrentThread()},
            base::BindLambdaForTesting([token, &channel]() {
              mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
                  token,
                  channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD());
            }));
      }));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(chromeos::MOUNT_ERROR_NONE));
  EXPECT_CALL(mock_delegate_, OnDisconnected()).Times(0);

  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kOk);
        EXPECT_TRUE(host);
        // Don't capture |host|. Its destruction will close the Mojo message
        // pipe and cause the child process to shut down gracefully.
        run_loop.Quit();
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);
  mounter->Mount(callback);

  run_loop.Run();

  EXPECT_TRUE(child_process.WaitForExit(nullptr));
}

}  // namespace
}  // namespace smbfs
