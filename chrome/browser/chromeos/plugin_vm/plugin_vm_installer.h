// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"

namespace download {
class DownloadService;
struct CompletionInfo;
}  // namespace download

class Profile;

namespace plugin_vm {

class PluginVmDriveImageDownloadService;

// PluginVmInstaller is responsible for installing the PluginVm image,
// including downloading this image from url specified by the user policy,
// and importing the downloaded image archive using concierge D-Bus services.
//
// This class uses one of two different objects for handling file downloads. If
// the image is hosted on Drive, a PluginVmDriveImageDownloadService object is
// used due to the need for using the Drive API. In all other cases, the
// DownloadService class is used to make the request directly.
class PluginVmInstaller : public KeyedService,
                          public chromeos::ConciergeClient::DiskImageObserver {
 public:
  // FailureReasons values can be shown to the user. Do not reorder or renumber
  // these values without careful consideration.
  enum class FailureReason {
    // LOGIC_ERROR = 0,
    SIGNAL_NOT_CONNECTED = 1,
    OPERATION_IN_PROGRESS = 2,
    NOT_ALLOWED = 3,
    INVALID_IMAGE_URL = 4,
    UNEXPECTED_DISK_IMAGE_STATUS = 5,
    INVALID_DISK_IMAGE_STATUS_RESPONSE = 6,
    DOWNLOAD_FAILED_UNKNOWN = 7,
    DOWNLOAD_FAILED_NETWORK = 8,
    DOWNLOAD_FAILED_ABORTED = 9,
    HASH_MISMATCH = 10,
    DISPATCHER_NOT_AVAILABLE = 11,
    CONCIERGE_NOT_AVAILABLE = 12,
    COULD_NOT_OPEN_IMAGE = 13,
    INVALID_IMPORT_RESPONSE = 14,
    IMAGE_IMPORT_FAILED = 15,
    DLC_DOWNLOAD_FAILED = 16,
    // DLC_DOWNLOAD_NOT_STARTED = 17,
  };

  // Observer class for the PluginVm image related events.
  class Observer {
   public:
    virtual ~Observer() = default;

    // If a VM already exists, we call this and abort the installation process.
    virtual void OnVmExists() = 0;

    virtual void OnDlcDownloadProgressUpdated(double progress,
                                              base::TimeDelta elapsed_time) = 0;
    virtual void OnDlcDownloadCompleted() = 0;
    virtual void OnDlcDownloadCancelled() = 0;
    virtual void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                           int64_t content_length,
                                           base::TimeDelta elapsed_time) = 0;
    virtual void OnDownloadCompleted() = 0;
    virtual void OnDownloadCancelled() = 0;
    virtual void OnDownloadFailed(FailureReason reason) = 0;
    virtual void OnImportProgressUpdated(int percent_completed,
                                         base::TimeDelta elapsed_time) = 0;
    virtual void OnImported() = 0;
    virtual void OnImportCancelled() = 0;
    virtual void OnImportFailed(FailureReason reason) = 0;
  };

  explicit PluginVmInstaller(Profile* profile);

  // Returns true if installer is processing a PluginVm image at the moment.
  bool IsProcessing();

  // Start the installation. Progress updates will be sent to the observer.
  void Start();
  // Cancel the installation.
  void Cancel();

  void SetObserver(Observer* observer);
  void RemoveObserver();

  // Called by DlcserviceClient, are not supposed to be used by other classes.
  void OnDlcDownloadProgressUpdated(double progress);
  void OnDlcDownloadCompleted(const std::string& err,
                              const dlcservice::DlcModuleList& dlc_module_list);

  // Called by PluginVmImageDownloadClient, are not supposed to be used by other
  // classes.
  void OnDownloadStarted();
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length);
  void OnDownloadCompleted(const download::CompletionInfo& info);
  void OnDownloadCancelled();
  void OnDownloadFailed(FailureReason reason);

  // ConciergeClient::DiskImageObserver:
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override;

  // Helper function that returns true in case downloaded PluginVm image
  // archive passes hash verification and false otherwise.
  // Public for testing purposes.
  bool VerifyDownload(const std::string& downloaded_archive_hash);

  void SetDownloadServiceForTesting(
      download::DownloadService* download_service);
  void SetDownloadedPluginVmImageArchiveForTesting(
      const base::FilePath& downloaded_plugin_vm_image_archive);
  void SetDriveDownloadServiceForTesting(
      std::unique_ptr<PluginVmDriveImageDownloadService>
          drive_download_service);
  std::string GetCurrentDownloadGuidForTesting();

 private:
  void OnUpdateVmState(bool default_vm_exists);
  void StartDlcDownload();
  void StartDownload();
  void StartImport();

  // DLC(s) cannot be currently cancelled when initiated, so this will cause
  // progress and completed install callbacks to be blocked to the observer if
  // there is an install taking place.
  void CancelDlcDownload();
  // Cancels the download of PluginVm image finishing the image processing.
  // Downloaded PluginVm image archive is being deleted.
  void CancelDownload();
  // Makes a call to concierge to cancel the import.
  void CancelImport();

  enum class State {
    NOT_STARTED,
    DOWNLOADING_DLC,
    DOWNLOAD_DLC_CANCELLED,
    DOWNLOADING,
    DOWNLOAD_CANCELLED,
    IMPORTING,
    IMPORT_CANCELLED,
    // TODO(timloh): We treat these all the same as NOT_STARTED. Consider
    // merging these together.
    CONFIGURED,
    DOWNLOAD_DLC_FAILED,
    DOWNLOAD_FAILED,
    IMPORT_FAILED,
  };

  Profile* profile_ = nullptr;
  Observer* observer_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  State state_ = State::NOT_STARTED;
  std::string current_download_guid_;
  base::FilePath downloaded_plugin_vm_image_archive_;
  dlcservice::DlcModuleList dlc_module_list_;
  // Used to identify our running import with concierge:
  std::string current_import_command_uuid_;
  // -1 when is not yet determined.
  int64_t downloaded_plugin_vm_image_size_ = -1;
  base::TimeTicks dlc_download_start_tick_;
  base::TimeTicks download_start_tick_;
  base::TimeTicks import_start_tick_;
  std::unique_ptr<PluginVmDriveImageDownloadService> drive_download_service_;
  bool using_drive_download_service_ = false;

  ~PluginVmInstaller() override;

  // Get string representation of state for logging purposes.
  std::string GetStateName(State state);

  GURL GetPluginVmImageDownloadUrl();
  download::DownloadParams GetDownloadParams(const GURL& url);

  void OnStartDownload(const std::string& download_guid,
                       download::DownloadParams::StartResult start_result);

  // Callback when PluginVm dispatcher is started (together with supporting
  // services such as concierge). This will then make the call to concierge's
  // ImportDiskImage.
  void OnPluginVmDispatcherStarted(bool success);

  // Callback which is called once we know if concierge is available.
  void OnConciergeAvailable(bool success);

  // Ran as a blocking task preparing the FD for the ImportDiskImage call.
  base::Optional<base::ScopedFD> PrepareFD();

  // Callback when the FD is prepared. Makes the call to ImportDiskImage.
  void OnFDPrepared(base::Optional<base::ScopedFD> maybeFd);

  // Callback for the concierge DiskImageImport call.
  void OnImportDiskImage(
      base::Optional<vm_tools::concierge::ImportDiskImageResponse> reply);

  // After we get a signal that the import is finished successfully, we
  // make one final call to concierge's DiskImageStatus method to get a
  // final resolution.
  void RequestFinalStatus();

  // Callback for the final call to concierge's DiskImageStatus to
  // get the final result of the disk import operation. This moves
  // the installer to a finishing state, depending on the result of the
  // query. Called when the signal for the command indicates that we
  // are done with importing.
  void OnFinalDiskImageStatus(
      base::Optional<vm_tools::concierge::DiskImageStatusResponse> reply);

  // Finishes the processing of PluginVm image. If |failure_reason| has a value,
  // then the import has failed, otherwise it was successful.
  void OnImported(base::Optional<FailureReason> failure_reason);

  // Callback for the concierge CancelDiskImageOperation call.
  void OnImportDiskImageCancelled(
      base::Optional<vm_tools::concierge::CancelDiskImageResponse> reply);

  void RemoveTemporaryPluginVmImageArchiveIfExists();
  void OnTemporaryPluginVmImageArchiveRemoved(bool success);

  base::WeakPtrFactory<PluginVmInstaller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmInstaller);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_
