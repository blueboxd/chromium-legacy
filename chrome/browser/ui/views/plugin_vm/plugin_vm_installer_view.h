// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_INSTALLER_VIEW_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_installer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class ImageView;
class Label;
class ProgressBar;
}  // namespace views

class Profile;

// The front end for Plugin VM, shown the first time the user launches it.
class PluginVmInstallerView : public views::BubbleDialogDelegateView,
                              public plugin_vm::PluginVmInstaller::Observer {
 public:
  explicit PluginVmInstallerView(Profile* profile);

  static PluginVmInstallerView* GetActiveViewForTesting();

  // views::BubbleDialogDelegateView implementation.
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

  // plugin_vm::PluginVmImageDownload::Observer implementation.
  void OnProgressUpdated(double fraction_complete) override;
  void OnCheckedDiskSpace(bool low_disk_space) override;
  void OnDlcDownloadCompleted() override;
  void OnExistingVmCheckCompleted(bool has_vm) override;
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length) override;
  void OnDownloadCompleted() override;
  void OnCreated() override;
  void OnImported() override;
  void OnError(plugin_vm::PluginVmInstaller::FailureReason reason) override;
  void OnCancelFinished() override;

  // Public for testing purposes.
  base::string16 GetBigMessage() const;
  base::string16 GetMessage() const;

  void SetFinishedCallbackForTesting(
      base::OnceCallback<void(bool success)> callback);

 private:
  // TODO(crbug.com/1063748): Re-use PluginVmInstaller::InstallingState.
  enum class State {
    STARTING,  // View was just created, installation hasn't yet started
    CHECKING_DISK_SPACE,  // Checking there is available free disk space.
    LOW_DISK_SPACE,   // Prompt user to continue or abort due to low disk space.
    DOWNLOADING_DLC,  // PluginVm DLC downloading and installing in progress.
    CHECKING_VMS,     // Checking for existing VMs.
    DOWNLOADING,      // Image download (ISO or VM) is in progress.
    IMPORTING,        // Downloaded image is being imported.
    CREATED,          // A brand new VM has been created using ISO image.
    IMPORTED,         // Downloaded VM image has been imported successfully.
    ERROR,            // Something unexpected happened.
  };

  ~PluginVmInstallerView() override;

  int GetCurrentDialogButtons() const;
  base::string16 GetCurrentDialogButtonLabel(ui::DialogButton button) const;

  void OnStateUpdated();
  // views::BubbleDialogDelegateView implementation.
  void AddedToWidget() override;

  base::string16 GetDownloadProgressMessage(uint64_t downlaoded_bytes,
                                            int64_t content_length) const;
  void SetBigMessageLabel();
  void SetMessageLabel();
  void SetBigImage();

  void StartInstallation();

  Profile* profile_ = nullptr;
  base::string16 app_name_;
  plugin_vm::PluginVmInstaller* plugin_vm_installer_ = nullptr;
  views::Label* big_message_label_ = nullptr;
  views::Label* message_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;
  views::Label* download_progress_message_label_ = nullptr;
  views::ImageView* big_image_ = nullptr;
  base::TimeTicks setup_start_tick_;

  State state_ = State::STARTING;
  base::Optional<plugin_vm::PluginVmInstaller::FailureReason> reason_;

  base::OnceCallback<void(bool success)> finished_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmInstallerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_INSTALLER_VIEW_H_
