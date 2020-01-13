// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smbfs_share.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace chromeos {
namespace smb_client {

namespace {

constexpr char kMountDirPrefix[] = "smbfs-";

SmbMountResult MountErrorToMountResult(smbfs::mojom::MountError mount_error) {
  switch (mount_error) {
    case smbfs::mojom::MountError::kOk:
      return SmbMountResult::SUCCESS;
    case smbfs::mojom::MountError::kTimeout:
      return SmbMountResult::ABORTED;
    case smbfs::mojom::MountError::kInvalidUrl:
      return SmbMountResult::INVALID_URL;
    case smbfs::mojom::MountError::kInvalidOptions:
      return SmbMountResult::INVALID_OPERATION;
    case smbfs::mojom::MountError::kNotFound:
      return SmbMountResult::NOT_FOUND;
    case smbfs::mojom::MountError::kAccessDenied:
      return SmbMountResult::AUTHENTICATION_FAILED;
    case smbfs::mojom::MountError::kInvalidProtocol:
      return SmbMountResult::UNSUPPORTED_DEVICE;
    case smbfs::mojom::MountError::kUnknown:
    default:
      return SmbMountResult::UNKNOWN_FAILURE;
  }
}

}  // namespace

SmbFsShare::SmbFsShare(Profile* profile,
                       const std::string& share_path,
                       const std::string& display_name,
                       const MountOptions& options)
    : profile_(profile),
      share_path_(share_path),
      display_name_(display_name),
      options_(options),
      mount_id_(
          base::ToLowerASCII(base::UnguessableToken::Create().ToString())) {}

SmbFsShare::~SmbFsShare() {
  Unmount();
}

void SmbFsShare::Mount(SmbFsShare::MountCallback callback) {
  DCHECK(!mounter_);
  DCHECK(!host_);

  // TODO(amistry): Come up with a scheme for consistent mount paths between
  // sessions.
  const std::string mount_dir = base::StrCat({kMountDirPrefix, mount_id_});
  if (mounter_creation_callback_for_test_) {
    mounter_ = mounter_creation_callback_for_test_.Run(share_path_, mount_dir,
                                                       options_, this);
  } else {
    mounter_ = std::make_unique<smbfs::SmbFsMounter>(
        share_path_, mount_dir, options_, this,
        chromeos::disks::DiskMountManager::GetInstance());
  }
  mounter_->Mount(base::BindOnce(&SmbFsShare::OnMountDone,
                                 base::Unretained(this), std::move(callback)));
}

void SmbFsShare::Unmount() {
  // Cancel any pending mount request.
  mounter_.reset();

  if (!host_) {
    return;
  }

  // Remove volume from VolumeManager.
  file_manager::VolumeManager::Get(profile_)->RemoveSmbFsVolume(
      host_->mount_path());

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  bool success = mount_points->RevokeFileSystem(mount_id_);
  CHECK(success);

  host_.reset();
}

void SmbFsShare::OnMountDone(MountCallback callback,
                             smbfs::mojom::MountError mount_error,
                             std::unique_ptr<smbfs::SmbFsHost> smbfs_host) {
  // Don't need the mounter any more.
  mounter_.reset();

  if (mount_error != smbfs::mojom::MountError::kOk) {
    std::move(callback).Run(MountErrorToMountResult(mount_error));
    return;
  }

  DCHECK(smbfs_host);
  host_ = std::move(smbfs_host);

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  bool success = mount_points->RegisterFileSystem(
      mount_id_, storage::kFileSystemTypeSmbFs,
      storage::FileSystemMountOption(), host_->mount_path());
  CHECK(success);

  file_manager::VolumeManager::Get(profile_)->AddSmbFsVolume(
      host_->mount_path(), display_name_);
  std::move(callback).Run(SmbMountResult::SUCCESS);
}

void SmbFsShare::OnDisconnected() {
  Unmount();
}

void SmbFsShare::SetMounterCreationCallbackForTest(
    MounterCreationCallback callback) {
  mounter_creation_callback_for_test_ = std::move(callback);
}

}  // namespace smb_client
}  // namespace chromeos
