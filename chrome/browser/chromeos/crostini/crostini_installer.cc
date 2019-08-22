// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_installer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

namespace crostini {

namespace {

using Error = CrostiniInstallerUIDelegate::Error;
using SetupResult = CrostiniInstaller::SetupResult;
using InstallationState = CrostiniInstallerUIDelegate::InstallationState;

class CrostiniInstallerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static crostini::CrostiniInstaller* GetForProfile(Profile* profile) {
    return static_cast<crostini::CrostiniInstaller*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniInstallerFactory* GetInstance() {
    static base::NoDestructor<CrostiniInstallerFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniInstallerFactory>;

  CrostiniInstallerFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniInstallerService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(crostini::CrostiniManagerFactory::GetInstance());
  }

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new crostini::CrostiniInstaller(profile);
  }
};

constexpr int kUninitializedDiskSpace = -1;

constexpr char kCrostiniSetupResultHistogram[] = "Crostini.SetupResult";
constexpr char kCrostiniTimeFromDeviceSetupToInstall[] =
    "Crostini.TimeFromDeviceSetupToInstall";
constexpr char kCrostiniDiskImageSizeHistogram[] = "Crostini.DiskImageSize";
constexpr char kCrostiniTimeToInstallSuccess[] =
    "Crostini.TimeToInstallSuccess";
constexpr char kCrostiniTimeToInstallCancel[] = "Crostini.TimeToInstallCancel";
constexpr char kCrostiniTimeToInstallError[] = "Crostini.TimeToInstallError";
constexpr char kCrostiniAvailableDiskSuccess[] =
    "Crostini.AvailableDiskSuccess";
constexpr char kCrostiniAvailableDiskCancel[] = "Crostini.AvailableDiskCancel";
constexpr char kCrostiniAvailableDiskError[] = "Crostini.AvailableDiskError";

void RecordTimeFromDeviceSetupToInstallMetric() {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation),
      base::BindOnce([](base::TimeDelta time_from_device_setup) {
        if (time_from_device_setup.is_zero())
          return;

        // The magic number 1471228928 is used for legacy reasons and changing
        // it would invalidate already logged data.
        base::UmaHistogramCustomTimes(
            kCrostiniTimeFromDeviceSetupToInstall, time_from_device_setup,
            base::TimeDelta::FromMinutes(1),
            base::TimeDelta::FromMilliseconds(1471228928), 50);
      }));
}

SetupResult ErrorToSetupResult(Error error) {
  switch (error) {
    case Error::NONE:
      return SetupResult::kSuccess;
    case Error::ERROR_LOADING_TERMINA:
      return SetupResult::kErrorLoadingTermina;
    case Error::ERROR_STARTING_CONCIERGE:
      return SetupResult::kErrorStartingConcierge;
    case Error::ERROR_CREATING_DISK_IMAGE:
      return SetupResult::kErrorCreatingDiskImage;
    case Error::ERROR_STARTING_TERMINA:
      return SetupResult::kErrorStartingTermina;
    case Error::ERROR_STARTING_CONTAINER:
      return SetupResult::kErrorStartingContainer;
    case Error::ERROR_OFFLINE:
      return SetupResult::kErrorOffline;
    case Error::ERROR_FETCHING_SSH_KEYS:
      return SetupResult::kErrorFetchingSshKeys;
    case Error::ERROR_MOUNTING_CONTAINER:
      return SetupResult::kErrorMountingContainer;
    case Error::ERROR_SETTING_UP_CONTAINER:
      return SetupResult::kErrorSettingUpContainer;
    case Error::ERROR_INSUFFICIENT_DISK_SPACE:
      return SetupResult::kErrorInsufficientDiskSpace;
  }

  NOTREACHED();
}

SetupResult InstallationStateToCancelledSetupResult(
    InstallationState installing_state) {
  switch (installing_state) {
    case InstallationState::START:
      return SetupResult::kUserCancelledStart;
    case InstallationState::INSTALL_IMAGE_LOADER:
      return SetupResult::kUserCancelledInstallImageLoader;
    case InstallationState::START_CONCIERGE:
      return SetupResult::kUserCancelledStartConcierge;
    case InstallationState::CREATE_DISK_IMAGE:
      return SetupResult::kUserCancelledCreateDiskImage;
    case InstallationState::START_TERMINA_VM:
      return SetupResult::kUserCancelledStartTerminaVm;
    case InstallationState::CREATE_CONTAINER:
      return SetupResult::kUserCancelledCreateContainer;
    case InstallationState::SETUP_CONTAINER:
      return SetupResult::kUserCancelledSetupContainer;
    case InstallationState::START_CONTAINER:
      return SetupResult::kUserCancelledStartContainer;
    case InstallationState::FETCH_SSH_KEYS:
      return SetupResult::kUserCancelledFetchSshKeys;
    case InstallationState::MOUNT_CONTAINER:
      return SetupResult::kUserCancelledMountContainer;
  }

  NOTREACHED();
}

}  // namespace

CrostiniInstaller* CrostiniInstaller::GetForProfile(Profile* profile) {
  return CrostiniInstallerFactory::GetForProfile(profile);
}

CrostiniInstaller::CrostiniInstaller(Profile* profile) : profile_(profile) {}

CrostiniInstaller::~CrostiniInstaller() {
  // Guaranteed by |Shutdown()|.
  DCHECK_EQ(restart_id_, CrostiniManager::kUninitializedRestartId);
}

void CrostiniInstaller::Shutdown() {
  if (restart_id_ != CrostiniManager::kUninitializedRestartId) {
    CrostiniManager::GetForProfile(profile_)->AbortRestartCrostini(
        restart_id_, base::DoNothing());
    restart_id_ = CrostiniManager::kUninitializedRestartId;
  }
}

void CrostiniInstaller::Install(ProgressCallback progress_callback,
                                ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanInstall()) {
    LOG(ERROR)
        << "Tried to start crostini installation in invalid state. state_="
        << static_cast<int>(state_);
    return;
  }

  progress_callback_ = std::move(progress_callback);
  result_callback_ = std::move(result_callback);

  install_start_time_ = base::TimeTicks::Now();
  require_cleanup_ = true;
  free_disk_space_ = kUninitializedDiskSpace;
  container_download_percent_ = 0;
  UpdateState(State::INSTALLING);

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&CrostiniInstaller::OnAvailableDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));

  // The default value of kCrostiniContainers is set to migrate existing
  // crostini users who don't have the pref set. If crostini is being installed,
  // then we know the user must not actually have any containers yet, so we set
  // this pref to the empty list.
  profile_->GetPrefs()->Set(crostini::prefs::kCrostiniContainers,
                            base::Value(base::Value::Type::LIST));
}

void CrostiniInstaller::Cancel(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ != State::INSTALLING) {
    LOG(ERROR) << "Tried to cancel in non-cancelable state. state_="
               << static_cast<int>(state_);
    return;
  }

  UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallCancel,
                           base::TimeTicks::Now() - install_start_time_);

  result_callback_.Reset();
  progress_callback_.Reset();
  cancel_callback_ = std::move(callback);

  if (installing_state_ == InstallationState::START) {
    // We have not called |RestartCrostini()| yet.
    DCHECK_EQ(restart_id_, CrostiniManager::kUninitializedRestartId);
    // OnAvailableDiskSpace() will take care of |cancel_callback_|.
    UpdateState(State::CANCEL_ABORT_CHECK_DISK);
    return;
  }

  DCHECK_NE(restart_id_, CrostiniManager::kUninitializedRestartId);

  if (free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskCancel,
                               free_disk_space_ >> 20);
  }

  // Abort the long-running flow, and RestartObserver methods will not be called
  // again until next installation.
  crostini::CrostiniManager::GetForProfile(profile_)->AbortRestartCrostini(
      restart_id_, base::DoNothing());
  restart_id_ = CrostiniManager::kUninitializedRestartId;
  RecordSetupResult(InstallationStateToCancelledSetupResult(installing_state_));

  if (require_cleanup_) {
    // Remove anything that got installed
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&crostini::CrostiniManager::RemoveCrostini,
                       base::Unretained(
                           crostini::CrostiniManager::GetForProfile(profile_)),
                       crostini::kCrostiniDefaultVmName,
                       base::BindOnce(&CrostiniInstaller::FinishCleanup,
                                      weak_ptr_factory_.GetWeakPtr())));
    UpdateState(State::CANCEL_CLEANUP);
  } else {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   std::move(cancel_callback_));
    UpdateState(State::IDLE);
  }
}

void CrostiniInstaller::CancelBeforeStart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanInstall()) {
    LOG(ERROR) << "Not in pre-install state. state_="
               << static_cast<int>(state_);
    return;
  }
  RecordSetupResult(SetupResult::kNotStarted);
}

void CrostiniInstaller::OnComponentLoaded(CrostiniResult result) {
  DCHECK_EQ(installing_state_, InstallationState::INSTALL_IMAGE_LOADER);

  if (result != CrostiniResult::SUCCESS) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while downloading cros-termina";
      HandleError(Error::ERROR_OFFLINE);
    } else {
      LOG(ERROR) << "Failed to install the cros-termina component";
      HandleError(Error::ERROR_LOADING_TERMINA);
    }
    return;
  }
  UpdateInstallingState(InstallationState::START_CONCIERGE);
}

void CrostiniInstaller::OnConciergeStarted(bool success) {
  DCHECK_EQ(installing_state_, InstallationState::START_CONCIERGE);
  if (!success) {
    HandleError(Error::ERROR_STARTING_CONCIERGE);
    return;
  }
  UpdateInstallingState(InstallationState::CREATE_DISK_IMAGE);
}

void CrostiniInstaller::OnDiskImageCreated(
    bool success,
    vm_tools::concierge::DiskImageStatus status,
    int64_t disk_size_available) {
  DCHECK_EQ(installing_state_, InstallationState::CREATE_DISK_IMAGE);
  if (!success) {
    HandleError(Error::ERROR_CREATING_DISK_IMAGE);
    return;
  }
  if (status == vm_tools::concierge::DiskImageStatus::DISK_STATUS_EXISTS) {
    require_cleanup_ = false;
  } else {
    // Record the max space for the disk image at creation time, measured in GiB
    base::UmaHistogramCustomCounts(kCrostiniDiskImageSizeHistogram,
                                   disk_size_available >> 30, 1, 1024, 64);
  }
  UpdateInstallingState(InstallationState::START_TERMINA_VM);
}

void CrostiniInstaller::OnVmStarted(bool success) {
  DCHECK_EQ(installing_state_, InstallationState::START_TERMINA_VM);
  if (!success) {
    HandleError(Error::ERROR_STARTING_TERMINA);
    return;
  }
  UpdateInstallingState(InstallationState::CREATE_CONTAINER);
}

void CrostiniInstaller::OnContainerDownloading(int32_t download_percent) {
  DCHECK_EQ(installing_state_, InstallationState::CREATE_CONTAINER);
  container_download_percent_ = base::ClampToRange(download_percent, 0, 100);
  RunProgressCallback();
}

void CrostiniInstaller::OnContainerCreated(CrostiniResult result) {
  DCHECK_EQ(installing_state_, InstallationState::CREATE_CONTAINER);
  UpdateInstallingState(InstallationState::SETUP_CONTAINER);
}

void CrostiniInstaller::OnContainerSetup(bool success) {
  DCHECK_EQ(installing_state_, InstallationState::SETUP_CONTAINER);

  if (!success) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while downloading container";
      HandleError(Error::ERROR_OFFLINE);
    } else {
      HandleError(Error::ERROR_SETTING_UP_CONTAINER);
    }
    return;
  }
  UpdateInstallingState(InstallationState::START_CONTAINER);
}

void CrostiniInstaller::OnContainerStarted(CrostiniResult result) {
  DCHECK_EQ(installing_state_, InstallationState::START_CONTAINER);

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container with error code: "
               << static_cast<int>(result);
    HandleError(Error::ERROR_STARTING_CONTAINER);
    return;
  }
  UpdateInstallingState(InstallationState::FETCH_SSH_KEYS);
}

void CrostiniInstaller::OnSshKeysFetched(bool success) {
  DCHECK_EQ(installing_state_, InstallationState::FETCH_SSH_KEYS);

  if (!success) {
    HandleError(Error::ERROR_FETCHING_SSH_KEYS);
    return;
  }
  UpdateInstallingState(InstallationState::MOUNT_CONTAINER);
}

bool CrostiniInstaller::CanInstall() {
  // Allow to start from State::ERROR. In that case, we're doing a Retry.
  return state_ == State::IDLE || state_ == State::ERROR;
}

void CrostiniInstaller::RunProgressCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::INSTALLING);

  base::TimeDelta time_in_state =
      base::Time::Now() - installing_state_start_time_;

  double state_start_mark = 0;
  double state_end_mark = 0;
  int state_max_seconds = 1;

  switch (installing_state_) {
    case InstallationState::START:
      state_start_mark = 0;
      state_end_mark = 0;
      break;
    case InstallationState::INSTALL_IMAGE_LOADER:
      state_start_mark = 0.0;
      state_end_mark = 0.25;
      state_max_seconds = 30;
      break;
    case InstallationState::START_CONCIERGE:
      state_start_mark = 0.25;
      state_end_mark = 0.26;
      break;
    case InstallationState::CREATE_DISK_IMAGE:
      state_start_mark = 0.26;
      state_end_mark = 0.27;
      break;
    case InstallationState::START_TERMINA_VM:
      state_start_mark = 0.27;
      state_end_mark = 0.35;
      state_max_seconds = 8;
      break;
    case InstallationState::CREATE_CONTAINER:
      state_start_mark = 0.35;
      state_end_mark = 0.90;
      state_max_seconds = 180;
      break;
    case InstallationState::SETUP_CONTAINER:
      state_start_mark = 0.90;
      state_end_mark = 0.95;
      state_max_seconds = 8;
      break;
    case InstallationState::START_CONTAINER:
      state_start_mark = 0.95;
      state_end_mark = 0.99;
      state_max_seconds = 8;
      break;
    case InstallationState::FETCH_SSH_KEYS:
      state_start_mark = 0.99;
      state_end_mark = 1;
      break;
    case InstallationState::MOUNT_CONTAINER:
      state_start_mark = 1;
      state_end_mark = 1;
      break;
    default:
      NOTREACHED();
  }

  double state_fraction = time_in_state.InSecondsF() / state_max_seconds;

  if (installing_state_ == InstallationState::CREATE_CONTAINER) {
    // In CREATE_CONTAINER, consume half the progress bar with downloading,
    // the rest with time.
    state_fraction =
        0.5 * (state_fraction + 0.01 * container_download_percent_);
  }

  double progress =
      state_start_mark + base::ClampToRange(state_fraction, 0.0, 1.0) *
                             (state_end_mark - state_start_mark);
  progress_callback_.Run(installing_state_, progress);
}

void CrostiniInstaller::UpdateState(State new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(state_, new_state);

  state_ = new_state;
  if (state_ == State::INSTALLING) {
    // We are not calling the progress callback here because 1) there is nothing
    // interesting to report; 2) We reach here from |Install()|, so calling the
    // callback risk reentering |Install()|'s caller.
    UpdateInstallingState(InstallationState::START, /*run_callback=*/false);

    state_progress_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(500),
        base::BindRepeating(&CrostiniInstaller::RunProgressCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    state_progress_timer_.AbandonAndStop();
  }
}

void CrostiniInstaller::UpdateInstallingState(
    InstallationState new_installing_state,
    bool run_callback) {
  DCHECK_EQ(state_, State::INSTALLING);
  installing_state_start_time_ = base::Time::Now();
  installing_state_ = new_installing_state;

  if (run_callback) {
    RunProgressCallback();
  }
}

void CrostiniInstaller::HandleError(Error error) {
  DCHECK_EQ(state_, State::INSTALLING);
  DCHECK_NE(error, Error::NONE);

  UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallError,
                           base::TimeTicks::Now() - install_start_time_);
  if (free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskError,
                               free_disk_space_ >> 20);
  }

  RecordSetupResult(ErrorToSetupResult(error));

  // |restart_id_| is reset in |OnCrostiniRestartFinished()|.
  UpdateState(State::ERROR);

  std::move(result_callback_).Run(error);
  progress_callback_.Reset();
}

void CrostiniInstaller::FinishCleanup(crostini::CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to cleanup aborted crostini install";
  }
  UpdateState(State::IDLE);
  std::move(cancel_callback_).Run();
}

void CrostiniInstaller::RecordSetupResult(SetupResult result) {
  base::UmaHistogramEnumeration(kCrostiniSetupResultHistogram, result);
}

void CrostiniInstaller::OnCrostiniRestartFinished(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  restart_id_ = CrostiniManager::kUninitializedRestartId;

  if (result != CrostiniResult::SUCCESS) {
    if (state_ != State::ERROR) {
      DCHECK_EQ(state_, State::INSTALLING);
      LOG(ERROR) << "Failed to restart Crostini with error code: "
                 << static_cast<int>(result);
      // TODO(lxj): The error code here is probably incorrect. If
      // |CrostiniManager::CrostiniRestarter| failed to mount the container, it
      // still calls this function with |SUCCESS| (see
      // |CrostiniRestarter::OnMountEvent()|), so if we reach here (i.e.
      // |state_| has not been set to |ERROR| but this function receives a
      // failure result), something else is probably wrong.
      HandleError(Error::ERROR_MOUNTING_CONTAINER);
    }
    return;
  }

  DCHECK_EQ(installing_state_, InstallationState::MOUNT_CONTAINER);
  // Reset state to allow |Install()| again in case the user remove and
  // re-install crostini.
  UpdateState(State::IDLE);

  RecordSetupResult(SetupResult::kSuccess);
  crostini::CrostiniManager::GetForProfile(profile_)
      ->UpdateLaunchMetricsForEnterpriseReporting();
  RecordTimeFromDeviceSetupToInstallMetric();
  UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallSuccess,
                           base::TimeTicks::Now() - install_start_time_);
  if (free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskSuccess,
                               free_disk_space_ >> 20);
  }

  std::move(result_callback_).Run(Error::NONE);
  progress_callback_.Reset();

  if (!skip_launching_terminal_for_testing_) {
    crostini::LaunchContainerTerminal(
        profile_, crostini::kCrostiniDefaultVmName,
        crostini::kCrostiniDefaultContainerName, std::vector<std::string>());
  }
}

void CrostiniInstaller::OnAvailableDiskSpace(int64_t bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |Cancel()| might be called immediately after |Install()|.
  if (state_ == State::CANCEL_ABORT_CHECK_DISK) {
    UpdateState(State::IDLE);
    RecordSetupResult(SetupResult::kNotStarted);
    std::move(cancel_callback_).Run();
    return;
  }

  DCHECK_EQ(installing_state_, InstallationState::START);

  free_disk_space_ = bytes;
  // Don't enforce minimum disk size on dev box or trybots because
  // base::SysInfo::AmountOfFreeDiskSpace returns zero in testing.
  if (free_disk_space_ < kMinimumFreeDiskSpace &&
      base::SysInfo::IsRunningOnChromeOS()) {
    HandleError(Error::ERROR_INSUFFICIENT_DISK_SPACE);
    return;
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    HandleError(Error::ERROR_OFFLINE);
    return;
  }

  UpdateInstallingState(InstallationState::INSTALL_IMAGE_LOADER);

  // Kick off the Crostini Restart sequence. We will be added as an observer.
  restart_id_ =
      crostini::CrostiniManager::GetForProfile(profile_)->RestartCrostini(
          crostini::kCrostiniDefaultVmName,
          crostini::kCrostiniDefaultContainerName,
          base::BindOnce(&CrostiniInstaller::OnCrostiniRestartFinished,
                         weak_ptr_factory_.GetWeakPtr()),
          this);

  // |restart_id| will be invalid when |CrostiniManager::RestartCrostini()|
  // decides to fail immediately and calls |OnCrostiniRestartFinished()|, which
  // subsequently set |state_| to |ERROR|.
  DCHECK_EQ(restart_id_ == CrostiniManager::kUninitializedRestartId,
            state_ == State::ERROR);
}

}  // namespace crostini
