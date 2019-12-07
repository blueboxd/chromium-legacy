// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/smbfs/smbfs_mounter.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace smbfs {

namespace {
constexpr char kMessagePipeName[] = "smbfs-bootstrap";
constexpr char kMountUrlPrefix[] = "smbfs://";
constexpr base::TimeDelta kMountTimeout = base::TimeDelta::FromSeconds(20);
}  // namespace

SmbFsMounter::MountOptions::MountOptions() = default;

SmbFsMounter::MountOptions::MountOptions(const MountOptions&) = default;

SmbFsMounter::MountOptions::~MountOptions() = default;

SmbFsMounter::SmbFsMounter(
    const std::string& share_path,
    const std::string& mount_dir_name,
    const MountOptions& options,
    SmbFsHost::Delegate* delegate,
    chromeos::disks::DiskMountManager* disk_mount_manager)
    : share_path_(share_path),
      mount_dir_name_(mount_dir_name),
      options_(options),
      delegate_(delegate),
      disk_mount_manager_(disk_mount_manager),
      token_(base::UnguessableToken::Create()),
      mount_url_(base::StrCat({kMountUrlPrefix, token_.ToString()})) {
  DCHECK(delegate_);
  DCHECK(disk_mount_manager_);
}

SmbFsMounter::~SmbFsMounter() {
  if (mojo_fd_pending_) {
    mojo_bootstrap::PendingConnectionManager::Get()
        .CancelExpectedOpenIpcChannel(token_);
  }
  disk_mount_manager_->RemoveObserver(this);
}

void SmbFsMounter::Mount(SmbFsMounter::DoneCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  CHECK(!mojo_fd_pending_);

  callback_ = std::move(callback);
  mojo_bootstrap::PendingConnectionManager::Get().ExpectOpenIpcChannel(
      token_,
      base::BindOnce(&SmbFsMounter::OnIpcChannel, base::Unretained(this)));
  mojo_fd_pending_ = true;

  bootstrap_.Bind(mojo::PendingRemote<mojom::SmbFsBootstrap>(
      bootstrap_invitation_.AttachMessagePipe(kMessagePipeName),
      mojom::SmbFsBootstrap::Version_));
  bootstrap_.set_disconnect_handler(
      base::BindOnce(&SmbFsMounter::OnMojoDisconnect, base::Unretained(this)));

  disk_mount_manager_->AddObserver(this);
  disk_mount_manager_->MountPath(mount_url_, "", mount_dir_name_, {},
                                 chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                                 chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  mount_timer_.Start(
      FROM_HERE, kMountTimeout,
      base::BindOnce(&SmbFsMounter::OnMountTimeout, base::Unretained(this)));
}

void SmbFsMounter::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  if (!callback_) {
    // This can happen if the mount timeout expires and the callback is already
    // run with a timeout error.
    return;
  }

  if (mount_url_.empty() ||
      mount_info.mount_type != chromeos::MOUNT_TYPE_NETWORK_STORAGE ||
      mount_info.source_path != mount_url_ ||
      event != chromeos::disks::DiskMountManager::MOUNTING) {
    return;
  }

  disk_mount_manager_->RemoveObserver(this);

  if (error_code != chromeos::MOUNT_ERROR_NONE) {
    LOG(WARNING) << "smbfs mount error: " << error_code;
    ProcessMountError(mojom::MountError::kUnknown);
    return;
  }

  DCHECK(!mount_info.mount_path.empty());
  mount_path_ = mount_info.mount_path;

  mojom::MountOptionsPtr mount_options = mojom::MountOptions::New();
  mount_options->share_path = share_path_;
  mount_options->username = options_.username;
  mount_options->workgroup = options_.workgroup;
  mount_options->password = options_.password;
  mount_options->allow_ntlm = options_.allow_ntlm;

  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;
  mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver =
      delegate_remote.InitWithNewPipeAndPassReceiver();

  bootstrap_->MountShare(
      std::move(mount_options), std::move(delegate_remote),
      base::BindOnce(&SmbFsMounter::OnMountShare, base::Unretained(this),
                     std::move(delegate_receiver)));
}

void SmbFsMounter::OnIpcChannel(base::ScopedFD mojo_fd) {
  DCHECK(mojo_fd.is_valid());
  mojo::OutgoingInvitation::Send(
      std::move(bootstrap_invitation_), base::kNullProcessHandle,
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(mojo_fd))));
  mojo_fd_pending_ = false;
}

void SmbFsMounter::OnMountShare(
    mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver,
    mojom::MountError mount_error,
    mojo::PendingRemote<mojom::SmbFs> smbfs) {
  if (!callback_) {
    return;
  }

  if (mount_error != mojom::MountError::kOk) {
    LOG(WARNING) << "smbfs mount share error: " << mount_error;
    ProcessMountError(mount_error);
    return;
  }

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      base::FilePath(mount_path_), delegate_, disk_mount_manager_,
      mojo::Remote<mojom::SmbFs>(std::move(smbfs)),
      std::move(delegate_receiver));
  std::move(callback_).Run(mojom::MountError::kOk, std::move(host));
}

void SmbFsMounter::OnMojoDisconnect() {
  if (!callback_) {
    return;
  }

  LOG(WARNING) << "smbfs bootstrap disconnection";
  ProcessMountError(mojom::MountError::kUnknown);
}

void SmbFsMounter::OnMountTimeout() {
  if (!callback_) {
    return;
  }

  LOG(ERROR) << "smbfs mount timeout";
  ProcessMountError(mojom::MountError::kTimeout);
}

void SmbFsMounter::ProcessMountError(mojom::MountError mount_error) {
  if (!mount_path_.empty()) {
    disk_mount_manager_->UnmountPath(
        mount_path_, base::BindOnce([](chromeos::MountError error_code) {
          LOG_IF(WARNING, error_code != chromeos::MOUNT_ERROR_NONE)
              << "Error unmounting smbfs on setup failure: " << error_code;
        }));
    mount_path_ = {};
  }

  std::move(callback_).Run(mount_error, nullptr);
}

}  // namespace smbfs
