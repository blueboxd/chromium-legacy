// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_typography.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_client_view.h"

using crostini::CrostiniResult;

namespace {
CrostiniInstallerView* g_crostini_installer_view = nullptr;

// The size of the download for the VM image.
// TODO(timloh): This is just a placeholder.
constexpr int kDownloadSizeInBytes = 300 * 1024 * 1024;
// The minimum feasible size for a VM disk image.
constexpr int64_t kMinimumDiskSize = 1ll * 1024 * 1024 * 1024;  // 1 GiB
// Minimum amount of free disk space to install crostini successfully.
constexpr int kMinimumFreeDiskSpace = kDownloadSizeInBytes + kMinimumDiskSize;

constexpr int kUninitializedDiskSpace = -1;

constexpr gfx::Insets kOOBEButtonRowInsets(32, 64, 32, 64);
constexpr int kOOBEWindowWidth = 768;
// TODO(timloh): The button row's preferred height (48px) adds to this. I'm not
// sure where this actually comes from but since we plan to rewrite this dialog
// in WebUI soon, this constant just hard-coded here.
constexpr int kOOBEWindowHeight = 640 - 48;
constexpr int kLinuxIllustrationWidth = 448;
constexpr int kLinuxIllustrationHeight = 180;

constexpr char kCrostiniSetupResultHistogram[] = "Crostini.SetupResult";
constexpr char kCrostiniSetupSourceHistogram[] = "Crostini.SetupSource";
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

// Generates a Google Help URL which includes a "board type" parameter. Some
// help pages need to be adjusted depending on the type of CrOS device that is
// accessing the page.
base::string16 GetHelpUrlWithBoard(const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

void RecordTimeFromDeviceSetupToInstallMetric() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation),
      base::BindOnce([](base::TimeDelta time_from_device_setup) {
        if (time_from_device_setup.is_zero())
          return;

        base::UmaHistogramCustomTimes(kCrostiniTimeFromDeviceSetupToInstall,
                                      time_from_device_setup,
                                      base::TimeDelta::FromMinutes(1),
                                      base::TimeDelta::FromDays(365), 50);
      }));
}

}  // namespace

void crostini::ShowCrostiniInstallerView(
    Profile* profile,
    crostini::CrostiniUISurface ui_surface) {
  // Defensive check to prevent showing the installer when crostini is not
  // allowed.
  if (!IsCrostiniUIAllowedForProfile(profile)) {
    return;
  }
  base::UmaHistogramEnumeration(kCrostiniSetupSourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);
  return CrostiniInstallerView::Show(profile);
}

void CrostiniInstallerView::Show(Profile* profile) {
  DCHECK(crostini::IsCrostiniUIAllowedForProfile(profile));
  if (!g_crostini_installer_view) {
    g_crostini_installer_view = new CrostiniInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_crostini_installer_view,
                                              nullptr, nullptr);
  }
  g_crostini_installer_view->GetDialogClientView()->SetButtonRowInsets(
      kOOBEButtonRowInsets);
  g_crostini_installer_view->GetWidget()->GetRootView()->Layout();
  // We do our layout when the big message is at its biggest. Then we can
  // set it to the desired value.
  g_crostini_installer_view->SetBigMessageLabel();
  g_crostini_installer_view->GetWidget()->Show();

  crostini::CrostiniManager::GetForProfile(profile)->SetInstallerViewStatus(
      true);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(
          &CrostiniInstallerView::OnAvailableDiskSpace,
          g_crostini_installer_view->weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerView::OnAvailableDiskSpace(int64_t bytes) {
  free_disk_space_ = bytes;
  if (free_disk_space_callback_for_testing_) {
    std::move(free_disk_space_callback_for_testing_).Run();
  }
}

int CrostiniInstallerView::GetDialogButtons() const {
  if (state_ == State::PROMPT || state_ == State::ERROR) {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniInstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (state_ == State::ERROR) {
      return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_RETRY_BUTTON);
    }
    return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALL_BUTTON);
  }
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  if (state_ == State::INSTALL_END)
    return l10n_util::GetStringUTF16(IDS_APP_CLOSE);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

bool CrostiniInstallerView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL &&
      (state_ == State::CLEANUP || state_ == State::CLEANUP_FINISHED)) {
    return false;
  }
  return true;
}

bool CrostiniInstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniInstallerView::ShouldShowWindowTitle() const {
  return false;
}

void CrostiniInstallerView::PressAccept() {
  GetDialogClientView()->AcceptWindow();
}

bool CrostiniInstallerView::Accept() {
  // This dialog can be accepted from State::ERROR. In that case, we're doing a
  // Retry.
  DCHECK(state_ == State::PROMPT || state_ == State::ERROR);

  // Delay starting the install process until we can check if there's enough
  // disk space.
  if (free_disk_space_ == kUninitializedDiskSpace) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&CrostiniInstallerView::PressAccept,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(50));
    return false;
  }

  UpdateState(State::INSTALL_START);
  install_start_time_ = base::TimeTicks::Now();

  // The default value of kCrostiniContainers is set to migrate existing
  // crostini users who don't have the pref set. If crostini is being installed,
  // then we know the user must not actually have any containers yet, so we set
  // this pref to the empty list.
  profile_->GetPrefs()->Set(crostini::prefs::kCrostiniContainers,
                            base::Value(base::Value::Type::LIST));

  // |learn_more_link_| should only be present in State::PROMPT.
  delete learn_more_link_;
  learn_more_link_ = nullptr;

  // HandleError needs the |progress_bar_|, so we delay our Offline check until
  // it exists.
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    const base::string16 device_type = ui::GetChromeOSDeviceName();
    HandleError(l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_OFFLINE_ERROR,
                                           device_type),
                SetupResult::kErrorOffline);
    return false;  // should not close the dialog.
  }

  // Don't enforce minimum disk size on dev box or trybots because
  // base::SysInfo::AmountOfFreeDiskSpace returns zero in testing.
  if (free_disk_space_ < kMinimumFreeDiskSpace &&
      base::SysInfo::IsRunningOnChromeOS()) {
    HandleError(l10n_util::GetStringFUTF16(
                    IDS_CROSTINI_INSTALLER_INSUFFICIENT_DISK,
                    ui::FormatBytesWithUnits(kMinimumFreeDiskSpace,
                                             ui::DATA_UNITS_GIBIBYTE,
                                             /*show_units=*/true)),
                SetupResult::kErrorInsufficientDiskSpace);
    return false;  // should not close the dialog.
  }

  // Kick off the Crostini Restart sequence. We will be added as an observer.
  restart_id_ =
      crostini::CrostiniManager::GetForProfile(profile_)->RestartCrostini(
          crostini::kCrostiniDefaultVmName,
          crostini::kCrostiniDefaultContainerName,
          base::BindOnce(&CrostiniInstallerView::MountContainerFinished,
                         weak_ptr_factory_.GetWeakPtr()),
          this);
  UpdateState(State::INSTALL_IMAGE_LOADER);
  return false;
}

bool CrostiniInstallerView::Cancel() {
  if (!has_logged_timing_result_ &&
      (restart_id_ != crostini::CrostiniManager::kUninitializedRestartId ||
       state_ == State::INSTALL_START)) {
    UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallCancel,
                             base::TimeTicks::Now() - install_start_time_);
    has_logged_timing_result_ = true;
  }
  if (!has_logged_free_disk_result_ &&
      (restart_id_ != crostini::CrostiniManager::kUninitializedRestartId ||
       state_ == State::INSTALL_START) &&
      free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskCancel,
                               free_disk_space_ >> 20);
    has_logged_free_disk_result_ = true;
  }
  if (state_ != State::INSTALL_END && state_ != State::CLEANUP &&
      state_ != State::CLEANUP_FINISHED &&
      restart_id_ != crostini::CrostiniManager::kUninitializedRestartId) {
    // Abort the long-running flow, and prevent our RestartObserver methods
    // being called after "this" has been destroyed.
    crostini::CrostiniManager::GetForProfile(profile_)->AbortRestartCrostini(
        restart_id_, base::DoNothing());

    SetupResult result = SetupResult::kUserCancelledStart;
    result = static_cast<SetupResult>(static_cast<int>(result) +
                                      static_cast<int>(state_) -
                                      static_cast<int>(State::INSTALL_START));
    RecordSetupResultHistogram(result);

    if (do_cleanup_) {
      // Remove anything that got installed
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(
              &crostini::CrostiniManager::RemoveCrostini,
              base::Unretained(
                  crostini::CrostiniManager::GetForProfile(profile_)),
              crostini::kCrostiniDefaultVmName,
              base::BindOnce(&CrostiniInstallerView::FinishCleanup,
                             weak_ptr_factory_.GetWeakPtr())));
      UpdateState(State::CLEANUP);
    }
    return !do_cleanup_;
  } else if (state_ == State::CLEANUP) {
    return false;
  } else if (restart_id_ ==
             crostini::CrostiniManager::kUninitializedRestartId) {
    RecordSetupResultHistogram(SetupResult::kNotStarted);
  }
  return true;  // Should close the dialog
}

gfx::Size CrostiniInstallerView::CalculatePreferredSize() const {
  return gfx::Size(kOOBEWindowWidth, kOOBEWindowHeight);
}

void CrostiniInstallerView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, learn_more_link_);

  NavigateParams params(
      profile_, GURL(GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void CrostiniInstallerView::OnComponentLoaded(CrostiniResult result) {
  DCHECK_EQ(state_, State::INSTALL_IMAGE_LOADER);

  if (result != CrostiniResult::SUCCESS) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while downloading cros-termina";
      const base::string16 device_type = ui::GetChromeOSDeviceName();
      HandleError(l10n_util::GetStringFUTF16(
                      IDS_CROSTINI_INSTALLER_OFFLINE_ERROR, device_type),
                  SetupResult::kErrorOffline);
    } else {
      LOG(ERROR) << "Failed to install the cros-termina component";
      HandleError(
          l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_LOAD_TERMINA_ERROR),
          SetupResult::kErrorLoadingTermina);
    }
    return;
  }
  VLOG(1) << "cros-termina install success";
  UpdateState(State::START_CONCIERGE);
}

void CrostiniInstallerView::OnConciergeStarted(bool success) {
  DCHECK_EQ(state_, State::START_CONCIERGE);
  if (!success) {
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_START_CONCIERGE_ERROR),
        SetupResult::kErrorStartingConcierge);
    return;
  }
  VLOG(1) << "Concierge service started";
  UpdateState(State::CREATE_DISK_IMAGE);
}

void CrostiniInstallerView::OnDiskImageCreated(
    bool success,
    vm_tools::concierge::DiskImageStatus status,
    int64_t disk_size_available) {
  DCHECK_EQ(state_, State::CREATE_DISK_IMAGE);
  if (status == vm_tools::concierge::DiskImageStatus::DISK_STATUS_EXISTS) {
    do_cleanup_ = false;
  } else if (success) {
    // Record the max space for the disk image at creation time, measured in GiB
    base::UmaHistogramCustomCounts(kCrostiniDiskImageSizeHistogram,
                                   disk_size_available >> 30, 1, 1024, 64);
  }
  if (!success) {
    HandleError(l10n_util::GetStringUTF16(
                    IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_ERROR),
                SetupResult::kErrorCreatingDiskImage);
    return;
  }
  VLOG(1) << "Created crostini disk image";
  UpdateState(State::START_TERMINA_VM);
}

void CrostiniInstallerView::OnVmStarted(bool success) {
  DCHECK_EQ(state_, State::START_TERMINA_VM);
  if (!success) {
    HandleError(l10n_util::GetStringUTF16(
                    IDS_CROSTINI_INSTALLER_START_TERMINA_VM_ERROR),
                SetupResult::kErrorStartingTermina);
    return;
  }
  VLOG(1) << "Started Termina VM successfully";
  UpdateState(State::CREATE_CONTAINER);
}

void CrostiniInstallerView::OnContainerDownloading(int32_t download_percent) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::CREATE_CONTAINER);
  container_download_percent_ = base::ClampToRange(download_percent, 0, 100);
  StepProgress();
}

void CrostiniInstallerView::OnContainerCreated(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::CREATE_CONTAINER);
  UpdateState(State::SETUP_CONTAINER);
}

void CrostiniInstallerView::OnContainerSetup(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::SETUP_CONTAINER);

  if (!success) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while downloading container";
      const base::string16 device_type = ui::GetChromeOSDeviceName();
      HandleError(l10n_util::GetStringFUTF16(
                      IDS_CROSTINI_INSTALLER_OFFLINE_ERROR, device_type),
                  SetupResult::kErrorOffline);
    } else {
      HandleError(l10n_util::GetStringUTF16(
                      IDS_CROSTINI_INSTALLER_SETUP_CONTAINER_ERROR),
                  SetupResult::kErrorSettingUpContainer);
    }
    return;
  }
  VLOG(1) << "Set up container successfully";
  UpdateState(State::START_CONTAINER);
}

void CrostiniInstallerView::OnContainerStarted(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::START_CONTAINER);

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container with error code: "
               << static_cast<int>(result);
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_START_CONTAINER_ERROR),
        SetupResult::kErrorStartingContainer);
    return;
  }
  VLOG(1) << "Started container successfully";
  UpdateState(State::FETCH_SSH_KEYS);
}

void CrostiniInstallerView::OnSshKeysFetched(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::FETCH_SSH_KEYS);

  if (!success) {
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_FETCH_SSH_KEYS_ERROR),
        SetupResult::kErrorFetchingSshKeys);
    return;
  }
  VLOG(1) << "Fetched ssh keys successfully";
  UpdateState(State::MOUNT_CONTAINER);
}

// static
CrostiniInstallerView* CrostiniInstallerView::GetActiveViewForTesting() {
  return g_crostini_installer_view;
}

void CrostiniInstallerView::SetCloseCallbackForTesting(
    base::OnceClosure quit_closure) {
  quit_closure_for_testing_ = std::move(quit_closure);
}

void CrostiniInstallerView::SetProgressBarCallbackForTesting(
    base::RepeatingCallback<void(double)> callback) {
  progress_bar_callback_for_testing_ = callback;
}

void CrostiniInstallerView::SetGetFreeDiskSpaceCallbackForTesting(
    base::OnceClosure quit_closure) {
  if (free_disk_space_ == kUninitializedDiskSpace) {
    free_disk_space_callback_for_testing_ = std::move(quit_closure);
  } else {
    std::move(free_disk_space_callback_for_testing_).Run();
  }
}

CrostiniInstallerView::CrostiniInstallerView(Profile* profile)
    : profile_(profile),
      free_disk_space_(kUninitializedDiskSpace),
      weak_ptr_factory_(this) {
  // Layout constants from the spec.
  constexpr gfx::Insets kDialogInsets(60, 64, 0, 64);
  constexpr int kDialogSpacingVertical = 32;
  constexpr gfx::Size kLogoImageSize(32, 32);
  constexpr gfx::Insets kLowerContainerInsets(0, 0, 80, 0);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kDialogSpacingVertical));

  views::View* upper_container_view = new views::View();
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kDialogInsets,
      kDialogSpacingVertical));
  AddChildView(upper_container_view);

  views::View* lower_container_view = new views::View();
  views::BoxLayout* lower_container_layout =
      lower_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kLowerContainerInsets));
  AddChildView(lower_container_view);

  logo_image_ = new views::ImageView();
  logo_image_->SetImageSize(kLogoImageSize);
  logo_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_DEFAULT_32));
  logo_image_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  upper_container_view->AddChildView(logo_image_);

  const base::string16 device_type = ui::GetChromeOSDeviceName();

  big_message_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALLING),
      ash::AshTextContext::CONTEXT_HEADLINE_OVERSIZED);
  big_message_label_->SetMultiLine(true);
  big_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(big_message_label_);

  // TODO(timloh): Descenders in the message appear to be clipped, re-visit once
  // the UI has been fleshed out more.
  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_INSTALLER_BODY,
      ui::FormatBytesWithUnits(kDownloadSizeInBytes, ui::DATA_UNITS_MEBIBYTE,
                               /*show_units=*/true));

  // Make a view to keep |message_label_| and |learn_more_link_| together with
  // less vertical spacing than the other dialog views.
  views::View* message_view = new views::View();
  message_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  message_label_ = new views::Label(message);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_view->AddChildView(message_label_);

  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->set_listener(this);
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_view->AddChildView(learn_more_link_);

  upper_container_view->AddChildView(message_view);

  // Make a slot for the progress bar, but it's not initially visible.
  progress_bar_ = new views::ProgressBar();
  upper_container_view->AddChildView(progress_bar_);
  progress_bar_->SetVisible(false);

  big_image_ = new views::ImageView();
  big_image_->SetImageSize(
      gfx::Size(kLinuxIllustrationWidth, kLinuxIllustrationHeight));
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LINUX_ILLUSTRATION));
  lower_container_view->AddChildView(big_image_);

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_INSTALLER);
}

CrostiniInstallerView::~CrostiniInstallerView() {
  crostini::CrostiniManager::GetForProfile(profile_)->SetInstallerViewStatus(
      false);
  g_crostini_installer_view = nullptr;
  if (quit_closure_for_testing_) {
    std::move(quit_closure_for_testing_).Run();
  }
}

void CrostiniInstallerView::FinishCleanup(CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to cleanup aborted crostini install";
  }
  // Need to do this because GetWidget()->Close() calls Cancel(), and we don't
  // want to restart the cleanup process
  UpdateState(State::CLEANUP_FINISHED);
  GetWidget()->Close();
}

void CrostiniInstallerView::HandleError(const base::string16& error_message,
                                        SetupResult result) {
  // Only ever set the error once. This check is necessary as the
  // CrostiniManager can give multiple error callbacks. Only the first should be
  // shown to the user.
  if (state_ == State::ERROR)
    return;

  if (!has_logged_timing_result_) {
    UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallError,
                             base::TimeTicks::Now() - install_start_time_);
    has_logged_timing_result_ = true;
  }
  if (!has_logged_free_disk_result_ &&
      free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskError,
                               free_disk_space_ >> 20);
    has_logged_free_disk_result_ = true;
  }

  RecordSetupResultHistogram(result);
  restart_id_ = crostini::CrostiniManager::kUninitializedRestartId;
  UpdateState(State::ERROR);
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);

  // Remove the buttons so they get recreated with correct color and
  // highlighting. Without this it is possible for both buttons to be styled as
  // default buttons.
  delete GetDialogClientView()->ok_button();
  delete GetDialogClientView()->cancel_button();
  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();
}

void CrostiniInstallerView::MountContainerFinished(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to mount container with error code: "
               << static_cast<int>(result);
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_MOUNT_CONTAINER_ERROR),
        SetupResult::kErrorMountingContainer);
    return;
  }
  ShowLoginShell();
}

void CrostiniInstallerView::ShowLoginShell() {
  DCHECK_EQ(state_, State::MOUNT_CONTAINER);
  UpdateState(State::SHOW_LOGIN_SHELL);

  crostini::LaunchContainerTerminal(profile_, crostini::kCrostiniDefaultVmName,
                                    crostini::kCrostiniDefaultContainerName,
                                    std::vector<std::string>());

  RecordSetupResultHistogram(SetupResult::kSuccess);
  crostini::CrostiniManager::GetForProfile(profile_)
      ->UpdateLaunchMetricsForEnterpriseReporting();
  RecordTimeFromDeviceSetupToInstallMetric();
  if (!has_logged_timing_result_) {
    UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallSuccess,
                             base::TimeTicks::Now() - install_start_time_);
    has_logged_timing_result_ = true;
  }
  if (!has_logged_free_disk_result_ &&
      free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskSuccess,
                               free_disk_space_ >> 20);
    has_logged_free_disk_result_ = true;
  }
  GetWidget()->Close();
}

void CrostiniInstallerView::StepProgress() {
  base::TimeDelta time_in_state = base::Time::Now() - state_start_time_;

  VLOG(1) << "state_ = " << static_cast<int>(state_);

  double state_start_mark = 0;
  double state_end_mark = 0;
  int state_max_seconds = 1;

  switch (state_) {
    case State::INSTALL_START:
      state_start_mark = 0;
      state_end_mark = 0;
      break;
    case State::INSTALL_IMAGE_LOADER:
      state_start_mark = 0.0;
      state_end_mark = 0.25;
      state_max_seconds = 30;
      break;
    case State::START_CONCIERGE:
      state_start_mark = 0.25;
      state_end_mark = 0.26;
      break;
    case State::CREATE_DISK_IMAGE:
      state_start_mark = 0.26;
      state_end_mark = 0.27;
      break;
    case State::START_TERMINA_VM:
      state_start_mark = 0.27;
      state_end_mark = 0.35;
      state_max_seconds = 8;
      break;
    case State::CREATE_CONTAINER:
      state_start_mark = 0.35;
      state_end_mark = 0.90;
      state_max_seconds = 180;
      break;
    case State::SETUP_CONTAINER:
      state_start_mark = 0.90;
      state_end_mark = 0.95;
      state_max_seconds = 8;
      break;
    case State::START_CONTAINER:
      state_start_mark = 0.95;
      state_end_mark = 0.99;
      state_max_seconds = 8;
      break;
    case State::FETCH_SSH_KEYS:
      state_start_mark = 0.99;
      state_end_mark = 1;
      break;
    case State::MOUNT_CONTAINER:
      state_start_mark = 1;
      state_end_mark = 1;
      break;

    default:
      break;
  }

  if (State::INSTALL_START <= state_ && state_ < State::INSTALL_END) {
    double state_fraction = time_in_state.InSecondsF() / state_max_seconds;

    if (state_ == State::CREATE_CONTAINER) {
      // In CREATE_CONTAINER, consume half the progress bar with downloading,
      // the rest with time.
      state_fraction =
          0.5 * (state_fraction + 0.01 * container_download_percent_);
    }
    VLOG(1) << "start = " << state_start_mark << ", end = " << state_end_mark
            << ", fraction = " << state_fraction;
    progress_bar_->SetValue(state_start_mark +
                            base::ClampToRange(state_fraction, 0.0, 1.0) *
                                (state_end_mark - state_start_mark));
    progress_bar_->SetVisible(true);
    if (progress_bar_callback_for_testing_) {
      progress_bar_callback_for_testing_.Run(progress_bar_->current_value());
    }
  } else if (state_ == State::CLEANUP) {
    progress_bar_->SetValue(-1);
    progress_bar_->SetVisible(true);
  } else {
    progress_bar_->SetVisible(false);
  }
  SetMessageLabel();
  SetBigMessageLabel();
  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();
}

void CrostiniInstallerView::UpdateState(State new_state) {
  state_start_time_ = base::Time::Now();
  state_ = new_state;
  if (state_ == State::INSTALL_START) {
    state_progress_timer_ = std::make_unique<base::RepeatingTimer>();
    state_progress_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(500),
        base::BindRepeating(&CrostiniInstallerView::StepProgress,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (state_ < State::INSTALL_START || state_ >= State::INSTALL_END) {
    if (state_progress_timer_) {
      VLOG(1) << "Killing timer, state_ = " << static_cast<int>(state_);
      state_progress_timer_->AbandonAndStop();
    }
  }

  StepProgress();
}

void CrostiniInstallerView::SetMessageLabel() {
  int message_id = 0;
  // The States below refer to stages that have completed.
  // The messages selected refer to the next stage, now underway.
  switch (state_) {
    case State::INSTALL_IMAGE_LOADER:
      message_id = IDS_CROSTINI_INSTALLER_LOAD_TERMINA_MESSAGE;
      break;
    case State::START_CONCIERGE:
      message_id = IDS_CROSTINI_INSTALLER_START_CONCIERGE_MESSAGE;
      break;
    case State::CREATE_DISK_IMAGE:
      message_id = IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_MESSAGE;
      break;
    case State::START_TERMINA_VM:
      message_id = IDS_CROSTINI_INSTALLER_START_TERMINA_VM_MESSAGE;
      break;
    case State::CREATE_CONTAINER:
      message_id = IDS_CROSTINI_INSTALLER_START_CONTAINER_MESSAGE;
      break;
    case State::START_CONTAINER:
      message_id = IDS_CROSTINI_INSTALLER_START_CONTAINER_MESSAGE;
      break;
    case State::SETUP_CONTAINER:
      message_id = IDS_CROSTINI_INSTALLER_SETUP_CONTAINER_MESSAGE;
      break;
    case State::FETCH_SSH_KEYS:
      message_id = IDS_CROSTINI_INSTALLER_FETCH_SSH_KEYS_MESSAGE;
      break;
    case State::MOUNT_CONTAINER:
      message_id = IDS_CROSTINI_INSTALLER_MOUNT_CONTAINER_MESSAGE;
      break;
    case State::CLEANUP:
      message_id = IDS_CROSTINI_INSTALLER_CANCELING;
      break;
    default:
      break;
  }

  if (message_id == 0) {
    message_label_->SetVisible(false);
    return;
  }

  message_label_->SetText(l10n_util::GetStringUTF16(message_id));
  message_label_->SetVisible(true);
}

void CrostiniInstallerView::SetBigMessageLabel() {
  base::string16 message;
  switch (state_) {
    case State::PROMPT: {
      const base::string16 device_type = ui::GetChromeOSDeviceName();
      message =
          l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_TITLE, device_type);
    } break;
    case State::ERROR:
      message = l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_ERROR_TITLE);
      break;
    case State::INSTALL_END:
      message = l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_COMPLETE);
      break;
    case State::CLEANUP:
      message =
          l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_CANCELING_TITLE);
      break;

    default:
      message = l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALLING);
      break;
  }
  big_message_label_->SetText(message);
  big_message_label_->SetVisible(true);
}

void CrostiniInstallerView::RecordSetupResultHistogram(SetupResult result) {
  // Prevent multiple results being logged for a given setup flow. This can
  // happen due to multiple error callbacks happening in some cases, as well
  // as the user being able to hit Cancel after any errors occur.
  if (has_logged_result_)
    return;

  base::UmaHistogramEnumeration(kCrostiniSetupResultHistogram, result,
                                SetupResult::kCount);
  has_logged_result_ = true;
}
