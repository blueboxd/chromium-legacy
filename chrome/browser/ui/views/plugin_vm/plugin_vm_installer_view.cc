// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"

#include <memory>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

PluginVmInstallerView* g_plugin_vm_installer_view = nullptr;

constexpr gfx::Insets kButtonRowInsets(0, 64, 32, 64);
constexpr int kWindowWidth = 768;
constexpr int kWindowHeight = 636;

}  // namespace

void plugin_vm::ShowPluginVmInstallerView(Profile* profile) {
  if (!g_plugin_vm_installer_view) {
    g_plugin_vm_installer_view = new PluginVmInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_plugin_vm_installer_view,
                                              nullptr, nullptr);
  }
  g_plugin_vm_installer_view->SetButtonRowInsets(kButtonRowInsets);
  g_plugin_vm_installer_view->GetWidget()->Show();
}

PluginVmInstallerView::PluginVmInstallerView(Profile* profile)
    : profile_(profile),
      app_name_(l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)),
      plugin_vm_installer_(
          plugin_vm::PluginVmInstallerFactory::GetForProfile(profile)) {
  // Layout constants from the spec.
  gfx::Insets kDialogInsets(60, 64, 0, 64);
  constexpr gfx::Insets kLowerContainerInsets(12, 0, 52, 0);
  constexpr gfx::Size kLogoImageSize(32, 32);
  constexpr gfx::Size kBigImageSize(264, 264);
  constexpr int kTitleFontSize = 28;
  const gfx::FontList kTitleFont({"Google Sans"}, gfx::Font::NORMAL,
                                 kTitleFontSize, gfx::Font::Weight::NORMAL);
  constexpr int kTitleHeight = 64;
  constexpr int kMessageFontSize = 13;
  const gfx::FontList kMessageFont({"Roboto"}, gfx::Font::NORMAL,
                                   kMessageFontSize, gfx::Font::Weight::NORMAL);
  constexpr int kMessageHeight = 32;
  constexpr int kDownloadProgressMessageFontSize = 12;
  const gfx::FontList kDownloadProgressMessageFont(
      {"Roboto"}, gfx::Font::NORMAL, kDownloadProgressMessageFontSize,
      gfx::Font::Weight::NORMAL);
  constexpr int kDownloadProgressMessageHeight = 24;
  constexpr int kProgressBarHeight = 5;
  constexpr int kProgressBarTopMargin = 32;

  // Removed margins so dialog insets specify it instead.
  set_margins(gfx::Insets());

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kDialogInsets));

  views::View* upper_container_view = new views::View();
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  AddChildView(upper_container_view);

  views::View* lower_container_view = new views::View();
  views::BoxLayout* lower_container_layout =
      lower_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kLowerContainerInsets));
  AddChildView(lower_container_view);

  views::ImageView* logo_image = new views::ImageView();
  logo_image->SetImageSize(kLogoImageSize);
  logo_image->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_PLUGIN_VM_DEFAULT_32));
  logo_image->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  upper_container_view->AddChildView(logo_image);

  big_message_label_ = new views::Label(GetBigMessage(), {kTitleFont});
  big_message_label_->SetProperty(
      views::kMarginsKey, gfx::Insets(kTitleHeight - kTitleFontSize, 0, 0, 0));
  big_message_label_->SetMultiLine(false);
  big_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(big_message_label_);

  views::View* message_container_view = new views::View();
  message_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kMessageHeight - kMessageFontSize, 0, 0, 0)));
  upper_container_view->AddChildView(message_container_view);

  message_label_ = new views::Label(GetMessage(), {kMessageFont});
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_container_view->AddChildView(message_label_);

  progress_bar_ = new views::ProgressBar(kProgressBarHeight);
  progress_bar_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(kProgressBarTopMargin - kProgressBarHeight, 0, 0, 0));
  upper_container_view->AddChildView(progress_bar_);

  download_progress_message_label_ =
      new views::Label(base::string16(), {kDownloadProgressMessageFont});
  download_progress_message_label_->SetEnabledColor(gfx::kGoogleGrey700);
  download_progress_message_label_->SetProperty(
      views::kMarginsKey, gfx::Insets(kDownloadProgressMessageHeight -
                                          kDownloadProgressMessageFontSize,
                                      0, 0, 0));
  download_progress_message_label_->SetMultiLine(false);
  download_progress_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(download_progress_message_label_);

  big_image_ = new views::ImageView();
  big_image_->SetImageSize(kBigImageSize);
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PLUGIN_VM_INSTALLER));
  lower_container_view->AddChildView(big_image_);

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);
}

// static
PluginVmInstallerView* PluginVmInstallerView::GetActiveViewForTesting() {
  return g_plugin_vm_installer_view;
}

bool PluginVmInstallerView::ShouldShowWindowTitle() const {
  return false;
}

bool PluginVmInstallerView::Accept() {
  if (state_ == State::CREATED || state_ == State::IMPORTED) {
    // Launch button has been clicked.
    plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)->LaunchPluginVm(
        base::DoNothing());
    return true;
  }
  if (state_ == State::LOW_DISK_SPACE) {
    state_ = State::DOWNLOADING_DLC;
    OnStateUpdated();
    plugin_vm_installer_->Continue();
    return false;
  }
  DCHECK_EQ(state_, State::ERROR);
  // Retry button has been clicked to retry setting of PluginVm environment
  // after error occurred.
  StartInstallation();
  return false;
}

bool PluginVmInstallerView::Cancel() {
  switch (state_) {
    case State::STARTING:
    case State::CHECKING_DISK_SPACE:
      plugin_vm::RecordPluginVmSetupResultHistogram(
          plugin_vm::PluginVmSetupResult::kUserCancelledCheckingDiskSpace);
      break;
    case State::LOW_DISK_SPACE:
      plugin_vm::RecordPluginVmSetupResultHistogram(
          plugin_vm::PluginVmSetupResult::kUserCancelledLowDiskSpace);
      break;
    case State::DOWNLOADING_DLC:
      plugin_vm::RecordPluginVmSetupResultHistogram(
          plugin_vm::PluginVmSetupResult::kUserCancelledDownloadingPluginVmDlc);
      break;
    case State::CHECKING_VMS:
      plugin_vm::RecordPluginVmSetupResultHistogram(
          plugin_vm::PluginVmSetupResult::kUserCancelledCheckingForExistingVm);
      break;
    case State::DOWNLOADING:
      plugin_vm::RecordPluginVmSetupResultHistogram(
          plugin_vm::PluginVmSetupResult::
              kUserCancelledDownloadingPluginVmImage);
      break;
    case State::IMPORTING:
      plugin_vm::RecordPluginVmSetupResultHistogram(
          plugin_vm::PluginVmSetupResult::kUserCancelledImportingPluginVmImage);
      break;
    case State::CREATED:
    case State::IMPORTED:
    case State::ERROR:
      // Setup result has already been logged in these cases.
      return true;
    default:
      NOTREACHED();
  }

  plugin_vm_installer_->Cancel();

  return true;
}

gfx::Size PluginVmInstallerView::CalculatePreferredSize() const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

void PluginVmInstallerView::OnProgressUpdated(double fraction_complete) {
  progress_bar_->SetValue(fraction_complete);
}

void PluginVmInstallerView::OnCheckedDiskSpace(bool low_disk_space) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::CHECKING_DISK_SPACE);

  if (low_disk_space)
    state_ = State::LOW_DISK_SPACE;
  else
    state_ = State::DOWNLOADING_DLC;
  OnStateUpdated();
}

void PluginVmInstallerView::OnDlcDownloadCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::DOWNLOADING_DLC);

  state_ = State::CHECKING_VMS;
  OnStateUpdated();
}

void PluginVmInstallerView::OnExistingVmCheckCompleted(bool has_vm) {
  DCHECK_EQ(state_, State::CHECKING_VMS);

  if (has_vm) {
    // This case should only occur if the user manually installed a VM via vmc,
    // which is rare enough so we just re-use the regular success strings.
    state_ = State::IMPORTED;
    OnStateUpdated();

    plugin_vm::RecordPluginVmSetupResultHistogram(
        plugin_vm::PluginVmSetupResult::kVmAlreadyExists);
    plugin_vm::RecordPluginVmSetupTimeHistogram(base::TimeTicks::Now() -
                                                setup_start_tick_);
  } else {
    state_ = State::DOWNLOADING;
    OnStateUpdated();
  }
}

// TODO(timloh): Cancelling the installation immediately closes the dialog, but
// getting back to a clean state could take several seconds. If a user then
// re-opens the dialog, it could cause it to fail unexpectedly. We should make
// use of these callback to avoid this.
void PluginVmInstallerView::OnCancelFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PluginVmInstallerView::OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                                      int64_t content_length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::DOWNLOADING);

  download_progress_message_label_->SetText(
      GetDownloadProgressMessage(bytes_downloaded, content_length));
  download_progress_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
}

void PluginVmInstallerView::OnDownloadCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::DOWNLOADING);

  state_ = State::IMPORTING;
  OnStateUpdated();
}

void PluginVmInstallerView::OnImported() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::IMPORTING);

  state_ = State::IMPORTED;
  OnStateUpdated();

  plugin_vm::RecordPluginVmSetupResultHistogram(
      plugin_vm::PluginVmSetupResult::kSuccess);
  plugin_vm::RecordPluginVmSetupTimeHistogram(base::TimeTicks::Now() -
                                              setup_start_tick_);
}

void PluginVmInstallerView::OnCreated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::IMPORTING);

  state_ = State::CREATED;
  OnStateUpdated();

  plugin_vm::RecordPluginVmSetupResultHistogram(
      plugin_vm::PluginVmSetupResult::kSuccess);
  plugin_vm::RecordPluginVmSetupTimeHistogram(base::TimeTicks::Now() -
                                              setup_start_tick_);
}

void PluginVmInstallerView::OnError(
    plugin_vm::PluginVmInstaller::FailureReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  state_ = State::ERROR;
  reason_ = reason;
  OnStateUpdated();

  plugin_vm::RecordPluginVmSetupResultHistogram(
      plugin_vm::PluginVmSetupResult::kError);
}

base::string16 PluginVmInstallerView::GetBigMessage() const {
  switch (state_) {
    case State::STARTING:
    case State::CHECKING_DISK_SPACE:
    case State::LOW_DISK_SPACE:
    case State::DOWNLOADING_DLC:
    case State::CHECKING_VMS:
    case State::DOWNLOADING:
    case State::IMPORTING:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_ENVIRONMENT_SETTING_TITLE, app_name_);
    case State::CREATED:
    case State::IMPORTED:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_FINISHED_TITLE);
    case State::ERROR:
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_TITLE, app_name_);
        default:
          return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_ERROR_TITLE);
      }
  }
}

base::string16 PluginVmInstallerView::GetMessage() const {
  switch (state_) {
    case State::LOW_DISK_SPACE:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_LOW_DISK_SPACE_MESSAGE,
          ui::FormatBytesWithUnits(
              plugin_vm::PluginVmInstaller::kRecommendedFreeDiskSpace,
              ui::DATA_UNITS_GIBIBYTE,
              /*show_units=*/true),
          app_name_);
    case State::STARTING:
    case State::CHECKING_DISK_SPACE:
    case State::DOWNLOADING_DLC:
    case State::CHECKING_VMS:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_INSTALLER_START_DOWNLOADING_MESSAGE);
    case State::DOWNLOADING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_INSTALLER_DOWNLOADING_MESSAGE);
    case State::IMPORTING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_INSTALLER_IMPORTING_MESSAGE);
    case State::IMPORTED:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_IMPORTED_MESSAGE, app_name_);
    case State::CREATED:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_CREATED_MESSAGE);
    case State::ERROR:
      using Reason = plugin_vm::PluginVmInstaller::FailureReason;
      DCHECK(reason_);
      switch (*reason_) {
        default:
        case Reason::SIGNAL_NOT_CONNECTED:
        case Reason::OPERATION_IN_PROGRESS:
        case Reason::UNEXPECTED_DISK_IMAGE_STATUS:
        case Reason::INVALID_DISK_IMAGE_STATUS_RESPONSE:
        case Reason::DISPATCHER_NOT_AVAILABLE:
        case Reason::CONCIERGE_NOT_AVAILABLE:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_LOGIC_ERROR, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::NOT_ALLOWED:
        case Reason::DLC_UNSUPPORTED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_MESSAGE, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::INVALID_IMAGE_URL:
        case Reason::HASH_MISMATCH:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_CONFIG_ERROR, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::DOWNLOAD_FAILED_UNKNOWN:
        case Reason::DOWNLOAD_FAILED_NETWORK:
        case Reason::DOWNLOAD_FAILED_ABORTED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_DOWNLOAD_FAILED,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::COULD_NOT_OPEN_IMAGE:
        case Reason::INVALID_IMPORT_RESPONSE:
        case Reason::IMAGE_IMPORT_FAILED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_INSTALLING_FAILED,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        // DLC Failure Reasons.
        case Reason::DLC_INTERNAL:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_DLC_INTERNAL_FAILED_MESSAGE, app_name_);
        case Reason::DLC_BUSY:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_DLC_BUSY_FAILED_MESSAGE, app_name_);
        case Reason::DLC_NEED_REBOOT:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_DLC_NEED_REBOOT_FAILED_MESSAGE, app_name_);
        case Reason::INSUFFICIENT_DISK_SPACE:
        case Reason::DLC_NEED_SPACE:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSUFFICIENT_DISK_SPACE_MESSAGE,
              ui::FormatBytesWithUnits(
                  plugin_vm::PluginVmInstaller::kMinimumFreeDiskSpace,
                  ui::DATA_UNITS_GIBIBYTE,
                  /*show_units=*/true),
              app_name_,
              ui::FormatBytesWithUnits(
                  plugin_vm::PluginVmInstaller::kRecommendedFreeDiskSpace,
                  ui::DATA_UNITS_GIBIBYTE,
                  /*show_units=*/true));
      }
  }
}

void PluginVmInstallerView::SetFinishedCallbackForTesting(
    base::OnceCallback<void(bool success)> callback) {
  finished_callback_for_testing_ = std::move(callback);
}

PluginVmInstallerView::~PluginVmInstallerView() {
  plugin_vm_installer_->RemoveObserver();
  g_plugin_vm_installer_view = nullptr;
}

int PluginVmInstallerView::GetCurrentDialogButtons() const {
  switch (state_) {
    case State::STARTING:
    case State::CHECKING_DISK_SPACE:
    case State::DOWNLOADING_DLC:
    case State::CHECKING_VMS:
    case State::DOWNLOADING:
    case State::IMPORTING:
      return ui::DIALOG_BUTTON_CANCEL;
    case State::LOW_DISK_SPACE:
    case State::IMPORTED:
    case State::CREATED:
      return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
    case State::ERROR:
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED:
          return ui::DIALOG_BUTTON_CANCEL;
        default:
          return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
      }
  }
}

base::string16 PluginVmInstallerView::GetCurrentDialogButtonLabel(
    ui::DialogButton button) const {
  switch (state_) {
    case State::STARTING:
    case State::CHECKING_DISK_SPACE:
    case State::DOWNLOADING_DLC:
    case State::CHECKING_VMS:
    case State::DOWNLOADING:
    case State::IMPORTING: {
      DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    }
    case State::CREATED:
    case State::IMPORTED: {
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_PLUGIN_VM_INSTALLER_LAUNCH_BUTTON
                                         : IDS_APP_CLOSE);
    }
    case State::LOW_DISK_SPACE:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK
              ? IDS_PLUGIN_VM_INSTALLER_CONTINUE_BUTTON
              : IDS_APP_CANCEL);
    case State::ERROR: {
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED:
          DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
          return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
        default:
          return l10n_util::GetStringUTF16(
              button == ui::DIALOG_BUTTON_OK
                  ? IDS_PLUGIN_VM_INSTALLER_RETRY_BUTTON
                  : IDS_APP_CANCEL);
      }
    }
  }
}

void PluginVmInstallerView::AddedToWidget() {
  if (state_ == State::STARTING)
    StartInstallation();
  else
    OnStateUpdated();
}

void PluginVmInstallerView::OnStateUpdated() {
  SetBigMessageLabel();
  SetMessageLabel();
  SetBigImage();

  int buttons = GetCurrentDialogButtons();
  DialogDelegate::SetButtons(buttons);
  if (buttons & ui::DIALOG_BUTTON_OK) {
    DialogDelegate::SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        GetCurrentDialogButtonLabel(ui::DIALOG_BUTTON_OK));
  }
  if (buttons & ui::DIALOG_BUTTON_CANCEL) {
    DialogDelegate::SetButtonLabel(
        ui::DIALOG_BUTTON_CANCEL,
        GetCurrentDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
  }

  const bool progress_bar_visible =
      state_ == State::STARTING || state_ == State::CHECKING_DISK_SPACE ||
      state_ == State::DOWNLOADING_DLC || state_ == State::CHECKING_VMS ||
      state_ == State::DOWNLOADING || state_ == State::IMPORTING;
  progress_bar_->SetVisible(progress_bar_visible);

  const bool download_progress_message_label_visible =
      state_ == State::DOWNLOADING;
  download_progress_message_label_->SetVisible(
      download_progress_message_label_visible);

  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();

  if (state_ == State::CREATED || state_ == State::IMPORTED ||
      state_ == State::ERROR) {
    if (finished_callback_for_testing_)
      std::move(finished_callback_for_testing_).Run(state_ != State::ERROR);
  }
}

base::string16 PluginVmInstallerView::GetDownloadProgressMessage(
    uint64_t bytes_downloaded,
    int64_t content_length) const {
  DCHECK_EQ(state_, State::DOWNLOADING);

  // If download size isn't known |fraction_complete| should be empty.
  if (content_length > 0) {
    return l10n_util::GetStringFUTF16(
        IDS_PLUGIN_VM_INSTALLER_DOWNLOAD_PROGRESS_MESSAGE,
        ui::FormatBytesWithUnits(bytes_downloaded, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/false),
        ui::FormatBytesWithUnits(content_length, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/true));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PLUGIN_VM_INSTALLER_DOWNLOAD_PROGRESS_WITHOUT_DOWNLOAD_SIZE_MESSAGE,
        ui::FormatBytesWithUnits(bytes_downloaded, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/true));
  }
}

void PluginVmInstallerView::SetBigMessageLabel() {
  big_message_label_->SetText(GetBigMessage());
  big_message_label_->SetVisible(true);
  big_message_label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                               true);
}

void PluginVmInstallerView::SetMessageLabel() {
  message_label_->SetText(GetMessage());
  message_label_->SetVisible(true);
  message_label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                           true);
}

void PluginVmInstallerView::SetBigImage() {
  if (state_ == State::ERROR) {
    big_image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_PLUGIN_VM_INSTALLER_ERROR));
    return;
  }
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PLUGIN_VM_INSTALLER));
}

void PluginVmInstallerView::StartInstallation() {
  // In each case setup starts from this function (when dialog is opened or
  // retry button is clicked).
  setup_start_tick_ = base::TimeTicks::Now();

  state_ = State::CHECKING_DISK_SPACE;
  progress_bar_->SetValue(0);
  OnStateUpdated();

  plugin_vm_installer_->SetObserver(this);
  plugin_vm_installer_->Start();
}
