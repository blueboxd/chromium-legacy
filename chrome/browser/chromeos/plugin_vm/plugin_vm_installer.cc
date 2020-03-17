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
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_drive_image_download_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
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
  return State::NOT_STARTED < state_ && state_ < State::CONFIGURED;
}

void PluginVmInstaller::Start() {
  if (IsProcessing()) {
    LOG(ERROR) << "Download of a PluginVm image couldn't be started as"
               << " another PluginVm image is currently being processed "
               << "in state " << GetStateName(state_);
    OnDownloadFailed(FailureReason::OPERATION_IN_PROGRESS);
    return;
  }
  // Defensive check preventing any download attempts when PluginVm is
  // not allowed to run (this might happen in rare cases if PluginVm has
  // been disabled but the installer icon is still visible).
  if (!IsPluginVmAllowedForProfile(profile_)) {
    LOG(ERROR) << "Download of PluginVm image cannot be started because "
               << "the user is not allowed to run PluginVm";
    OnDownloadFailed(FailureReason::NOT_ALLOWED);
    return;
  }

  // If there's an existing VM, we can complete without running the install
  // flow.
  PluginVmManager::GetForProfile(profile_)->UpdateVmState(
      base::BindOnce(&PluginVmInstaller::OnUpdateVmState,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PluginVmInstaller::StartDlcDownload,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::Cancel() {
  switch (state_) {
    case State::DOWNLOADING_DLC:
      CancelDlcDownload();
      return;
    case State::DOWNLOADING:
      CancelDownload();
      return;
    case State::IMPORTING:
      CancelImport();
      return;
    default:
      LOG(ERROR) << "Tried to cancel installation from unexpected state "
                 << GetStateName(state_);
      return;
  }
}

void PluginVmInstaller::OnUpdateVmState(bool default_vm_exists) {
  if (default_vm_exists) {
    if (observer_)
      observer_->OnVmExists();
    profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                     true);
    state_ = State::CONFIGURED;
    return;
  }
  StartDlcDownload();
}

void PluginVmInstaller::StartDlcDownload() {
  state_ = State::DOWNLOADING_DLC;

  if (!GetPluginVmImageDownloadUrl().is_valid()) {
    OnDownloadFailed(FailureReason::INVALID_IMAGE_URL);
    return;
  }

  dlc_download_start_tick_ = base::TimeTicks::Now();

  chromeos::DlcserviceClient::Get()->Install(
      GetPluginVmDlcModuleList(),
      base::BindOnce(&PluginVmInstaller::OnDlcDownloadCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmInstaller::OnDlcDownloadProgressUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::CancelDlcDownload() {
  state_ = State::DOWNLOAD_DLC_CANCELLED;
}

void PluginVmInstaller::StartDownload() {
  DCHECK_EQ(state_, State::DOWNLOADING_DLC);
  state_ = State::DOWNLOADING;

  GURL url = GetPluginVmImageDownloadUrl();
  // This may have changed since running StartDlcDownload.
  if (!url.is_valid()) {
    OnDownloadFailed(FailureReason::INVALID_IMAGE_URL);
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
  state_ = State::DOWNLOAD_CANCELLED;

  if (using_drive_download_service_) {
    DCHECK(drive_download_service_);
    drive_download_service_->CancelDownload();
  } else {
    download_service_->CancelDownload(current_download_guid_);
  }
}

void PluginVmInstaller::OnDlcDownloadProgressUpdated(double progress) {
  if (state_ == State::DOWNLOAD_DLC_CANCELLED)
    return;
  DCHECK_EQ(state_, State::DOWNLOADING_DLC);

  if (observer_)
    observer_->OnDlcDownloadProgressUpdated(
        progress, base::TimeTicks::Now() - dlc_download_start_tick_);
}

void PluginVmInstaller::OnDlcDownloadCompleted(
    const std::string& err,
    const dlcservice::DlcModuleList& dlc_module_list) {
  if (state_ == State::DOWNLOAD_DLC_CANCELLED) {
    if (observer_)
      observer_->OnDlcDownloadCancelled();
    state_ = State::NOT_STARTED;
    return;
  }
  DCHECK_EQ(state_, State::DOWNLOADING_DLC);

  // If success, continue to the next state.
  if (err == dlcservice::kErrorNone) {
    RecordPluginVmDlcUseResultHistogram(PluginVmDlcUseResult::kDlcSuccess);
    if (observer_)
      observer_->OnDlcDownloadCompleted();
    StartDownload();
    return;
  }

  // At this point, PluginVM DLC download failed.
  state_ = State::DOWNLOAD_DLC_FAILED;
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
  if (observer_)
    observer_->OnDownloadFailed(reason);
}

void PluginVmInstaller::OnDownloadStarted() {
  download_start_tick_ = base::TimeTicks::Now();
}

void PluginVmInstaller::OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                                  int64_t content_length) {
  if (observer_) {
    observer_->OnDownloadProgressUpdated(
        bytes_downloaded, content_length,
        base::TimeTicks::Now() - download_start_tick_);
  }
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
  DCHECK_EQ(state_, State::DOWNLOAD_CANCELLED);

  RemoveTemporaryImageIfExists();
  current_download_guid_.clear();
  if (using_drive_download_service_) {
    drive_download_service_->ResetState();
    using_drive_download_service_ = false;
  }
  if (observer_)
    observer_->OnDownloadCancelled();

  state_ = State::NOT_STARTED;
}

void PluginVmInstaller::OnDownloadFailed(FailureReason reason) {
  state_ = State::DOWNLOAD_FAILED;
  RemoveTemporaryImageIfExists();
  current_download_guid_.clear();

  if (using_drive_download_service_) {
    drive_download_service_->ResetState();
    using_drive_download_service_ = false;
  }

  if (observer_)
    observer_->OnDownloadFailed(reason);
}

void PluginVmInstaller::StartImport() {
  DCHECK_EQ(state_, State::DOWNLOADING);
  state_ = State::IMPORTING;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&PluginVmInstaller::DetectImageType,
                     base::Unretained(this)),
      base::BindOnce(&PluginVmInstaller::OnImageTypeDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::DetectImageType() {
  creating_new_vm_ = IsIsoImage(downloaded_image_);
}

void PluginVmInstaller::OnImageTypeDetected() {
  VLOG(1) << "Starting PluginVm dispatcher service";
  chromeos::DBusThreadManager::Get()
      ->GetDebugDaemonClient()
      ->StartPluginVmDispatcher(
          chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_),
          base::BindOnce(&PluginVmInstaller::OnPluginVmDispatcherStarted,
                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmInstaller::OnPluginVmDispatcherStarted(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start PluginVm dispatcher service";
    OnImported(FailureReason::DISPATCHER_NOT_AVAILABLE);
    return;
  }
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
  if (state_ == State::IMPORT_CANCELLED || state_ == State::NOT_STARTED)
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
  if (state_ == State::IMPORT_CANCELLED || state_ == State::NOT_STARTED)
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
  import_start_tick_ = base::TimeTicks::Now();
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
      if (observer_) {
        observer_->OnImportProgressUpdated(
            percent_completed, base::TimeTicks::Now() - import_start_tick_);
      }
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
    state_ = State::IMPORT_FAILED;
    if (observer_) {
      observer_->OnImportFailed(*failure_reason);
    }

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

  state_ = State::CONFIGURED;
}

void PluginVmInstaller::CancelImport() {
  state_ = State::IMPORT_CANCELLED;
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
  DCHECK_EQ(state_, State::IMPORT_CANCELLED);

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

  if (observer_)
    observer_->OnImportCancelled();
  state_ = State::NOT_STARTED;
  VLOG(1) << "Import disk image request has been cancelled successfully";
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
    case State::NOT_STARTED:
      return "NOT_STARTED";
    case State::DOWNLOADING_DLC:
      return "DOWNLOADING_DLC";
    case State::DOWNLOAD_DLC_CANCELLED:
      return "DOWNLOAD_DLC_CANCELLED";
    case State::DOWNLOADING:
      return "DOWNLOADING";
    case State::DOWNLOAD_CANCELLED:
      return "DOWNLOAD_CANCELLED";
    case State::IMPORTING:
      return "IMPORTING";
    case State::IMPORT_CANCELLED:
      return "IMPORT_CANCELLED";
    case State::CONFIGURED:
      return "CONFIGURED";
    case State::DOWNLOAD_DLC_FAILED:
      return "DOWNLOAD_DLC_FAILED";
    case State::DOWNLOAD_FAILED:
      return "DOWNLOAD_FAILED";
    case State::IMPORT_FAILED:
      return "IMPORT_FAILED";
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

}  // namespace plugin_vm
