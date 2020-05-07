// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_installer.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_drive_image_download_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

// Size to use for calculating progress when the actual size isn't available.
constexpr int64_t kDownloadSizeFallbackEstimate = 15LL * 1024 * 1024 * 1024;

constexpr char kFailureReasonHistogram[] = "PluginVm.SetupFailureReason";

constexpr char kHomeDirectory[] = "/home";

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

constexpr char kIsoSignature[] = "CD001";
constexpr int64_t kIsoOffsets[] = {0x8001, 0x8801, 0x9001};

bool IsIsoImage(const base::FilePath& image) {
  base::File file(image, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << image.value();
    return false;
  }

  std::vector<uint8_t> data(strlen(kIsoSignature));
  for (auto offset : kIsoOffsets) {
    if (file.ReadAndCheck(offset, data) &&
        std::string(data.begin(), data.end()) == kIsoSignature) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace plugin_vm {

PluginVmInstaller::~PluginVmInstaller() = default;

bool PluginVmInstaller::IsProcessing() {
  return state_ != State::kIdle;
}

void PluginVmInstaller::Start() {
  if (IsProcessing()) {
    LOG(ERROR) << "Download of a PluginVm image couldn't be started as"
               << " another PluginVm image is currently being processed "
               << "in state " << GetStateName(state_) << ", "
               << GetInstallingStateName(installing_state_);
    InstallFailed(FailureReason::OPERATION_IN_PROGRESS);
    return;
  }
  // Defensive check preventing any download attempts when PluginVm is
  // not allowed to run (this might happen in rare cases if PluginVm has
  // been disabled but the installer icon is still visible).
  if (!IsPluginVmAllowedForProfile(profile_)) {
    LOG(ERROR) << "Download of PluginVm image cannot be started because "
               << "the user is not allowed to run PluginVm";
    InstallFailed(FailureReason::NOT_ALLOWED);
    return;
  }

  state_ = State::kInstalling;
  installing_state_ = InstallingState::kCheckingDiskSpace;
  progress_ = 0;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(kHomeDirectory)),
      base::BindOnce(&PluginVmInstaller::OnAvailableDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::Continue() {
  if (state_ != State::kInstalling ||
      installing_state_ != InstallingState::kPausedLowDiskSpace) {
    LOG(ERROR) << "Tried to continue installation in unexpected state "
               << GetStateName(state_) << ", "
               << GetInstallingStateName(installing_state_);
    return;
  }

  StartDlcDownload();
}

void PluginVmInstaller::Cancel() {
  if (state_ != State::kInstalling) {
    LOG(ERROR) << "Tried to cancel installation from unexpected state "
               << GetStateName(state_);
    return;
  }
  state_ = State::kCancelling;
  switch (installing_state_) {
    case InstallingState::kPausedLowDiskSpace:
      CancelFinished();
      return;
    case InstallingState::kCheckingDiskSpace:
    case InstallingState::kCheckingForExistingVm:
    case InstallingState::kDownloadingDlc:
      // These can't be cancelled, so we wait for completion. For DLC, we also
      // block progress callbacks.
      return;
    case InstallingState::kDownloadingImage:
      CancelDownload();
      return;
    case InstallingState::kImporting:
      CancelImport();
      return;
    default:
      NOTREACHED();
  }
}

void PluginVmInstaller::OnAvailableDiskSpace(int64_t bytes) {
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  if (free_disk_space_for_testing_ != -1)
    bytes = free_disk_space_for_testing_;

  // We allow the installer to fail for users who already set up a VM via vmc
  // and have low disk space as it's simpler to check for existing VMs after
  // installing DLC and this case should be very rare.

  if (bytes < kMinimumFreeDiskSpace) {
    InstallFailed(FailureReason::INSUFFICIENT_DISK_SPACE);
    return;
  }

  if (bytes < kRecommendedFreeDiskSpace) {
    // If there's no observer, we would get stuck in the paused state.
    if (!observer_) {
      InstallFinished();
      return;
    }
    observer_->OnCheckedDiskSpace(/*low_disk_space=*/true);
    installing_state_ = InstallingState::kPausedLowDiskSpace;
    return;
  }

  if (observer_)
    observer_->OnCheckedDiskSpace(/*low_disk_space=*/false);
  StartDlcDownload();
}

void PluginVmInstaller::OnUpdateVmState(bool default_vm_exists) {
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  if (default_vm_exists) {
    if (observer_)
      observer_->OnExistingVmCheckCompleted(/*has_vm=*/true);
    profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                     true);
    InstallFinished();
    return;
  }

  if (observer_)
    observer_->OnExistingVmCheckCompleted(/*has_vm=*/false);
  StartDownload();
}

void PluginVmInstaller::OnUpdateVmStateFailed() {
  // Either the dispatcher failed to start or ListVms didn't work.
  // PluginVmManager logs the details.
  InstallFailed(FailureReason::DISPATCHER_NOT_AVAILABLE);
}

void PluginVmInstaller::StartDlcDownload() {
  installing_state_ = InstallingState::kDownloadingDlc;

  if (!GetPluginVmImageDownloadUrl().is_valid()) {
    InstallFailed(FailureReason::INVALID_IMAGE_URL);
    return;
  }

  chromeos::DlcserviceClient::Get()->Install(
      GetPluginVmDlcModuleList(),
      base::BindOnce(&PluginVmInstaller::OnDlcDownloadCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmInstaller::OnDlcDownloadProgressUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::StartDownload() {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingDlc);
  installing_state_ = InstallingState::kDownloadingImage;
  UpdateProgress(/*state_progress=*/0);

  GURL url = GetPluginVmImageDownloadUrl();
  // This may have changed since running StartDlcDownload.
  if (!url.is_valid()) {
    InstallFailed(FailureReason::INVALID_IMAGE_URL);
    return;
  }

  using_drive_download_service_ = IsDriveUrl(url);

  if (using_drive_download_service_) {
    if (!drive_download_service_) {
      drive_download_service_ =
          std::make_unique<PluginVmDriveImageDownloadService>(this, profile_);
    } else {
      drive_download_service_->ResetState();
    }

    drive_download_service_->StartDownload(GetIdFromDriveUrl(url));
  } else {
    download_service_->StartDownload(GetDownloadParams(url));
  }
}

void PluginVmInstaller::CancelDownload() {
  if (using_drive_download_service_) {
    DCHECK(drive_download_service_);
    drive_download_service_->CancelDownload();
    CancelFinished();
  } else {
    // OnDownloadCancelled() is called after the download is cancelled.
    download_service_->CancelDownload(current_download_guid_);
  }
}

void PluginVmInstaller::OnDlcDownloadProgressUpdated(double progress) {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingDlc);
  if (state_ == State::kCancelling)
    return;

  UpdateProgress(progress);
}

void PluginVmInstaller::OnDlcDownloadCompleted(
    const std::string& err,
    const dlcservice::DlcModuleList& dlc_module_list) {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingDlc);
  if (state_ == State::kCancelling) {
    CancelFinished();
    return;
  }

  // If success, continue to the next state.
  if (err == dlcservice::kErrorNone) {
    RecordPluginVmDlcUseResultHistogram(PluginVmDlcUseResult::kDlcSuccess);
    if (observer_)
      observer_->OnDlcDownloadCompleted();

    PluginVmManagerFactory::GetForProfile(profile_)->UpdateVmState(
        base::BindOnce(&PluginVmInstaller::OnUpdateVmState,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&PluginVmInstaller::OnUpdateVmStateFailed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // At this point, PluginVM DLC download failed.
  PluginVmDlcUseResult result = PluginVmDlcUseResult::kInternalDlcError;
  FailureReason reason = FailureReason::DLC_INTERNAL;

  if (err == dlcservice::kErrorInvalidDlc) {
    LOG(ERROR) << "PluginVM DLC is not supported, need to enable PluginVM DLC.";
    result = PluginVmDlcUseResult::kInvalidDlcError;
    reason = FailureReason::DLC_UNSUPPORTED;
  } else if (err == dlcservice::kErrorBusy) {
    LOG(ERROR)
        << "PluginVM DLC is not able to be downloaded as dlcservice is busy.";
    result = PluginVmDlcUseResult::kBusyDlcError;
    reason = FailureReason::DLC_BUSY;
  } else if (err == dlcservice::kErrorNeedReboot) {
    LOG(ERROR)
        << "Device has pending update and needs a reboot to use PluginVM DLC.";
    result = PluginVmDlcUseResult::kNeedRebootDlcError;
    reason = FailureReason::DLC_NEED_REBOOT;
  } else if (err == dlcservice::kErrorAllocation) {
    LOG(ERROR) << "Device needs to free space to use PluginVM DLC.";
    result = PluginVmDlcUseResult::kNeedSpaceDlcError;
    reason = FailureReason::DLC_NEED_SPACE;
  } else {
    LOG(ERROR) << "Failed to download PluginVM DLC: " << err;
  }

  RecordPluginVmDlcUseResultHistogram(result);
  InstallFailed(reason);
}

void PluginVmInstaller::OnDownloadStarted() {
}

void PluginVmInstaller::OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                                  int64_t content_length) {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);
  if (observer_)
    observer_->OnDownloadProgressUpdated(bytes_downloaded, content_length);

  if (content_length <= 0)
    content_length = kDownloadSizeFallbackEstimate;

  UpdateProgress(
      std::min(1., static_cast<double>(bytes_downloaded) / content_length));
}

void PluginVmInstaller::OnDownloadCompleted(
    const download::CompletionInfo& info) {
  downloaded_image_ = info.path;
  downloaded_image_size_ = info.bytes_downloaded;
  current_download_guid_.clear();

  if (!VerifyDownload(info.hash256)) {
    LOG(ERROR) << "Downloaded PluginVm image archive hash doesn't match "
               << "hash specified by the PluginVmImage policy";
    OnDownloadFailed(FailureReason::HASH_MISMATCH);
    return;
  }

  if (observer_)
    observer_->OnDownloadCompleted();
  RecordPluginVmImageDownloadedSizeHistogram(info.bytes_downloaded);
  StartImport();
}

void PluginVmInstaller::OnDownloadCancelled() {
  DCHECK_EQ(state_, State::kCancelling);
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);

  RemoveTemporaryImageIfExists();
  current_download_guid_.clear();
  if (using_drive_download_service_) {
    drive_download_service_->ResetState();
    using_drive_download_service_ = false;
  }

  CancelFinished();
}

void PluginVmInstaller::OnDownloadFailed(FailureReason reason) {
  RemoveTemporaryImageIfExists();
  current_download_guid_.clear();

  if (using_drive_download_service_) {
    drive_download_service_->ResetState();
    using_drive_download_service_ = false;
  }

  InstallFailed(reason);
}

void PluginVmInstaller::StartImport() {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);
  installing_state_ = InstallingState::kImporting;
  UpdateProgress(/*state_progress=*/0);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&PluginVmInstaller::DetectImageType,
                     base::Unretained(this)),
      base::BindOnce(&PluginVmInstaller::OnImageTypeDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::UpdateProgress(double state_progress) {
  DCHECK_EQ(state_, State::kInstalling);
  if (state_progress < 0 || state_progress > 1) {
    LOG(ERROR) << "Unexpected progress value " << state_progress
               << " in installing state "
               << GetInstallingStateName(installing_state_);
    return;
  }

  double start_range = 0;
  double end_range = 0;
  switch (installing_state_) {
    case InstallingState::kDownloadingDlc:
      start_range = 0;
      end_range = 0.01;
      break;
    case InstallingState::kDownloadingImage:
      start_range = 0.01;
      end_range = 0.45;
      break;
    case InstallingState::kImporting:
      start_range = 0.45;
      end_range = 1;
      break;
    default:
      // Other states take a negligible amount of time so we don't send progress
      // updates.
      NOTREACHED();
  }

  double new_progress =
      start_range + (end_range - start_range) * state_progress;
  if (new_progress < progress_) {
    LOG(ERROR) << "Progress went backwards from " << progress_ << " to "
               << progress_;
    return;
  }

  progress_ = new_progress;
  if (observer_)
    observer_->OnProgressUpdated(new_progress);
}

void PluginVmInstaller::DetectImageType() {
  creating_new_vm_ = IsIsoImage(downloaded_image_);
}

void PluginVmInstaller::OnImageTypeDetected() {
  VLOG(1) << "Waiting for Concierge to be available";
  GetConciergeClient()->WaitForServiceToBeAvailable(
      base::BindOnce(&PluginVmInstaller::OnConciergeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnConciergeAvailable(bool success) {
  if (!success) {
    LOG(ERROR) << "Concierge did not become available";
    OnImported(FailureReason::CONCIERGE_NOT_AVAILABLE);
    return;
  }
  if (!GetConciergeClient()->IsDiskImageProgressSignalConnected()) {
    LOG(ERROR) << "Disk image progress signal is not connected";
    OnImported(FailureReason::SIGNAL_NOT_CONNECTED);
    return;
  }
  VLOG(1) << "Plugin VM dispatcher service has been started and disk image "
             "signals are connected";
  GetConciergeClient()->AddDiskImageObserver(this);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&PluginVmInstaller::PrepareFD, base::Unretained(this)),
      base::BindOnce(&PluginVmInstaller::OnFDPrepared,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::Optional<base::ScopedFD> PluginVmInstaller::PrepareFD() {
  // In case import has been cancelled meantime.
  if (state_ != State::kInstalling)
    return base::nullopt;

  base::File file(downloaded_image_,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << downloaded_image_.value();
    return base::nullopt;
  }

  return base::ScopedFD(file.TakePlatformFile());
}

void PluginVmInstaller::OnFDPrepared(base::Optional<base::ScopedFD> maybeFd) {
  // In case import has been cancelled meantime.
  if (state_ != State::kInstalling)
    return;

  if (!maybeFd.has_value()) {
    LOG(ERROR) << "Could not open downloaded image";
    OnImported(FailureReason::COULD_NOT_OPEN_IMAGE);
    return;
  }

  base::ScopedFD fd(std::move(maybeFd.value()));

  if (creating_new_vm_) {
    vm_tools::concierge::CreateDiskImageRequest request;
    request.set_cryptohome_id(
        chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_));
    request.set_disk_path(kPluginVmName);
    request.set_storage_location(
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);
    request.set_source_size(downloaded_image_size_);

    VLOG(1) << "Making call to concierge to set up VM from an ISO";

    GetConciergeClient()->CreateDiskImageWithFd(
        std::move(fd), request,
        base::BindOnce(&PluginVmInstaller::OnImportDiskImage<
                           vm_tools::concierge::CreateDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    vm_tools::concierge::ImportDiskImageRequest request;
    request.set_cryptohome_id(
        chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_));
    request.set_disk_path(kPluginVmName);
    request.set_storage_location(
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);
    request.set_source_size(downloaded_image_size_);

    VLOG(1) << "Making call to concierge to import disk image";

    GetConciergeClient()->ImportDiskImage(
        std::move(fd), request,
        base::BindOnce(&PluginVmInstaller::OnImportDiskImage<
                           vm_tools::concierge::ImportDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

template <typename ReplyType>
void PluginVmInstaller::OnImportDiskImage(base::Optional<ReplyType> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Could not retrieve response from Create/ImportDiskImage "
               << "call to concierge";
    OnImported(FailureReason::INVALID_IMPORT_RESPONSE);
    return;
  }

  ReplyType response = reply.value();

  // TODO(https://crbug.com/966397): handle cases where this jumps straight to
  // completed?
  // TODO(https://crbug.com/966396): Handle error case when image already
  // exists.
  if (response.status() !=
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS) {
    LOG(ERROR) << "Disk image is not in progress. Status: " << response.status()
               << ", " << response.failure_reason();
    OnImported(FailureReason::UNEXPECTED_DISK_IMAGE_STATUS);
    return;
  }

  VLOG(1) << "Disk image creation/import is now in progress";
  current_import_command_uuid_ = response.command_uuid();
  // Image in progress. Waiting for progress signals...
  // TODO(https://crbug.com/966398): think about adding a timeout here,
  //   i.e. what happens if concierge dies and does not report any signal
  //   back, not even an error signal. Right now, the user would see
  //   the "Configuring Plugin VM" screen forever. Maybe that's OK
  //   at this stage though.
}

void PluginVmInstaller::OnDiskImageProgress(
    const vm_tools::concierge::DiskImageStatusResponse& signal) {
  if (signal.command_uuid() != current_import_command_uuid_)
    return;

  const uint64_t percent_completed = signal.progress();
  const vm_tools::concierge::DiskImageStatus status = signal.status();

  switch (status) {
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED:
      VLOG(1) << "Disk image status indicates that importing is done.";
      RequestFinalStatus();
      return;
    case vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS:
      UpdateProgress(percent_completed / 100.);
      return;
    default:
      LOG(ERROR) << "Disk image status signal has status: " << status
                 << " with error message: " << signal.failure_reason()
                 << " and current progress: " << percent_completed;
      OnImported(FailureReason::UNEXPECTED_DISK_IMAGE_STATUS);
      return;
  }
}

void PluginVmInstaller::RequestFinalStatus() {
  vm_tools::concierge::DiskImageStatusRequest status_request;
  status_request.set_command_uuid(current_import_command_uuid_);
  GetConciergeClient()->DiskImageStatus(
      status_request, base::BindOnce(&PluginVmInstaller::OnFinalDiskImageStatus,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnFinalDiskImageStatus(
    base::Optional<vm_tools::concierge::DiskImageStatusResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Could not retrieve response from DiskImageStatus call to "
               << "concierge";
    OnImported(FailureReason::INVALID_DISK_IMAGE_STATUS_RESPONSE);
    return;
  }

  vm_tools::concierge::DiskImageStatusResponse response = reply.value();
  DCHECK(response.command_uuid() == current_import_command_uuid_);
  if (response.status() !=
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED) {
    LOG(ERROR) << "Disk image is not created. Status: " << response.status()
               << ", " << response.failure_reason();
    OnImported(FailureReason::IMAGE_IMPORT_FAILED);
    return;
  }

  OnImported(base::nullopt);
}

void PluginVmInstaller::OnImported(
    base::Optional<FailureReason> failure_reason) {
  GetConciergeClient()->RemoveDiskImageObserver(this);
  RemoveTemporaryImageIfExists();
  current_import_command_uuid_.clear();

  if (failure_reason) {
    if (creating_new_vm_)
      LOG(ERROR) << "New VM creation failed";
    else
      LOG(ERROR) << "Image import failed";
    InstallFailed(*failure_reason);
    return;
  }

  profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                   true);
  if (observer_) {
    if (creating_new_vm_)
      observer_->OnCreated();
    else
      observer_->OnImported();
  }
  InstallFinished();
}

void PluginVmInstaller::CancelImport() {
  VLOG(1) << "Cancelling disk image import with command_uuid: "
          << current_import_command_uuid_;

  vm_tools::concierge::CancelDiskImageRequest request;
  request.set_command_uuid(current_import_command_uuid_);
  GetConciergeClient()->CancelDiskImageOperation(
      request, base::BindOnce(&PluginVmInstaller::OnImportDiskImageCancelled,
                              weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnImportDiskImageCancelled(
    base::Optional<vm_tools::concierge::CancelDiskImageResponse> reply) {
  DCHECK_EQ(state_, State::kCancelling);
  DCHECK_EQ(installing_state_, InstallingState::kImporting);

  RemoveTemporaryImageIfExists();

  // TODO(https://crbug.com/966392): Handle unsuccessful PluginVm image
  // importing cancellation.
  if (!reply.has_value()) {
    LOG(ERROR) << "Could not retrieve response from CancelDiskImageOperation "
               << "call to concierge";
    return;
  }

  vm_tools::concierge::CancelDiskImageResponse response = reply.value();
  if (!response.success()) {
    LOG(ERROR) << "Import disk image request failed to be cancelled, "
               << response.failure_reason();
    return;
  }

  VLOG(1) << "Import disk image request has been cancelled successfully";
  CancelFinished();
}

void PluginVmInstaller::SetObserver(Observer* observer) {
  observer_ = observer;
}

void PluginVmInstaller::RemoveObserver() {
  observer_ = nullptr;
}

void PluginVmInstaller::SetDownloadServiceForTesting(
    download::DownloadService* download_service) {
  download_service_ = download_service;
}

void PluginVmInstaller::SetDownloadedImageForTesting(
    const base::FilePath& downloaded_image) {
  downloaded_image_ = downloaded_image;
}

std::string PluginVmInstaller::GetCurrentDownloadGuidForTesting() {
  return current_download_guid_;
}

void PluginVmInstaller::SetDriveDownloadServiceForTesting(
    std::unique_ptr<PluginVmDriveImageDownloadService> drive_download_service) {
  drive_download_service_ = std::move(drive_download_service);
}

PluginVmInstaller::PluginVmInstaller(Profile* profile)
    : profile_(profile),
      download_service_(
          DownloadServiceFactory::GetForKey(profile->GetProfileKey())) {}

GURL PluginVmInstaller::GetPluginVmImageDownloadUrl() {
  const base::Value* url_ptr =
      profile_->GetPrefs()
          ->GetDictionary(plugin_vm::prefs::kPluginVmImage)
          ->FindKey("url");
  if (!url_ptr) {
    LOG(ERROR) << "Url to PluginVm image is not specified";
    return GURL();
  }
  return GURL(url_ptr->GetString());
}

std::string PluginVmInstaller::GetStateName(State state) {
  switch (state) {
    case State::kIdle:
      return "kIdle";
    case State::kInstalling:
      return "kInstalling";
    case State::kCancelling:
      return "kCancelling";
  }
}

std::string PluginVmInstaller::GetInstallingStateName(InstallingState state) {
  switch (state) {
    case InstallingState::kInactive:
      return "kInactive";
    case InstallingState::kCheckingDiskSpace:
      return "kCheckingDiskSpace";
    case InstallingState::kPausedLowDiskSpace:
      return "kPausedLowDiskSpace";
    case InstallingState::kCheckingForExistingVm:
      return "kCheckingForExistingVm";
    case InstallingState::kDownloadingDlc:
      return "kDownloadingDlc";
    case InstallingState::kDownloadingImage:
      return "kDownloadingImage";
    case InstallingState::kImporting:
      return "kImporting";
  }
}

download::DownloadParams PluginVmInstaller::GetDownloadParams(const GURL& url) {
  download::DownloadParams params;

  // DownloadParams
  params.client = download::DownloadClient::PLUGIN_VM_IMAGE;
  params.guid = base::GenerateGUID();
  params.callback = base::BindRepeating(&PluginVmInstaller::OnStartDownload,
                                        weak_ptr_factory_.GetWeakPtr());

  params.traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kPluginVmNetworkTrafficAnnotation);

  // RequestParams
  params.request_params.url = url;
  params.request_params.method = "GET";

  // SchedulingParams
  // User initiates download by clicking on PluginVm icon so priorities should
  // be the highest.
  params.scheduling_params.priority = download::SchedulingParams::Priority::UI;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::NONE;

  return params;
}

void PluginVmInstaller::OnStartDownload(
    const std::string& download_guid,
    download::DownloadParams::StartResult start_result) {
  if (start_result == download::DownloadParams::ACCEPTED)
    current_download_guid_ = download_guid;
  else
    OnDownloadFailed(FailureReason::DOWNLOAD_FAILED_UNKNOWN);
}

bool PluginVmInstaller::VerifyDownload(
    const std::string& downloaded_archive_hash) {
  if (downloaded_archive_hash.empty()) {
    LOG(ERROR) << "No hash found for downloaded PluginVm image archive";
    return false;
  }
  const base::Value* plugin_vm_image_hash_ptr =
      profile_->GetPrefs()
          ->GetDictionary(plugin_vm::prefs::kPluginVmImage)
          ->FindKey("hash");
  if (!plugin_vm_image_hash_ptr) {
    LOG(ERROR) << "Hash of PluginVm image is not specified";
    return false;
  }
  std::string plugin_vm_image_hash = plugin_vm_image_hash_ptr->GetString();

  return base::EqualsCaseInsensitiveASCII(plugin_vm_image_hash,
                                          downloaded_archive_hash);
}

void PluginVmInstaller::RemoveTemporaryImageIfExists() {
  if (using_drive_download_service_) {
    drive_download_service_->RemoveTemporaryArchive(
        base::BindOnce(&PluginVmInstaller::OnTemporaryImageRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (!downloaded_image_.empty()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&base::DeleteFile, downloaded_image_,
                       false /* recursive */),
        base::BindOnce(&PluginVmInstaller::OnTemporaryImageRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PluginVmInstaller::OnTemporaryImageRemoved(bool success) {
  if (!success) {
    LOG(ERROR) << "Downloaded PluginVm image located in "
               << downloaded_image_.value() << " failed to be deleted";
    return;
  }
  downloaded_image_size_ = -1;
  downloaded_image_.clear();
  creating_new_vm_ = false;
}

void PluginVmInstaller::CancelFinished() {
  DCHECK_EQ(state_, State::kCancelling);
  state_ = State::kIdle;
  installing_state_ = InstallingState::kInactive;

  if (observer_)
    observer_->OnCancelFinished();
}

void PluginVmInstaller::InstallFailed(FailureReason reason) {
  state_ = State::kIdle;
  installing_state_ = InstallingState::kInactive;
  base::UmaHistogramEnumeration(kFailureReasonHistogram, reason);
  if (observer_)
    observer_->OnError(reason);
}

void PluginVmInstaller::InstallFinished() {
  state_ = State::kIdle;
  installing_state_ = InstallingState::kInactive;
}

}  // namespace plugin_vm
