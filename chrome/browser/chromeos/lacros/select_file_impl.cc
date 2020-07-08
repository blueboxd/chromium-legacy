// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/select_file_impl.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chromeos/lacros/mojom/select_file.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

ui::SelectFileDialog::Type GetUiType(lacros::mojom::SelectFileDialogType type) {
  switch (type) {
    case lacros::mojom::SelectFileDialogType::kFolder:
      return ui::SelectFileDialog::Type::SELECT_FOLDER;
    case lacros::mojom::SelectFileDialogType::kUploadFolder:
      return ui::SelectFileDialog::Type::SELECT_UPLOAD_FOLDER;
    case lacros::mojom::SelectFileDialogType::kExistingFolder:
      return ui::SelectFileDialog::Type::SELECT_EXISTING_FOLDER;
    case lacros::mojom::SelectFileDialogType::kOpenFile:
      return ui::SelectFileDialog::Type::SELECT_OPEN_FILE;
    case lacros::mojom::SelectFileDialogType::kOpenMultiFile:
      return ui::SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE;
    case lacros::mojom::SelectFileDialogType::kSaveAsFile:
      return ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE;
  }
}

// Manages a single open/save dialog. There may be multiple dialogs showing at
// the same time. Deletes itself when the dialog is closed.
class SelectFileDialogHolder : public ui::SelectFileDialog::Listener {
 public:
  SelectFileDialogHolder(lacros::mojom::SelectFileOptionsPtr options,
                         lacros::mojom::SelectFile::SelectCallback callback)
      : select_callback_(std::move(callback)) {
    // Policy is null because showing the file-dialog-blocked infobar is handled
    // client-side in lacros-chrome.
    select_file_dialog_ =
        ui::SelectFileDialog::Create(this, /*policy=*/nullptr);

    // TODO(https://crbug.com/1090587): Parent to the ShellSurface that spawned
    // the dialog. For now, just put it on the default desktop.
    aura::Window* owning_window = ash::Shell::GetContainer(
        ash::Shell::GetRootWindowForNewWindows(),
        ash::kShellWindowId_DefaultContainerDeprecated);

    // TODO(https://crbug.com/1090587): File type filter support.
    select_file_dialog_->SelectFile(
        GetUiType(options->type), options->title, options->default_path,
        /*file_types=*/nullptr,
        /*file_type_index=*/0,
        /*default_extension=*/std::string(), owning_window,
        /*params=*/nullptr);
  }

  SelectFileDialogHolder(const SelectFileDialogHolder&) = delete;
  SelectFileDialogHolder& operator=(const SelectFileDialogHolder&) = delete;
  ~SelectFileDialogHolder() override = default;

 private:
  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    OnSelected({path});
  }

  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    OnSelected(files);
  }

  void FileSelectionCanceled(void* params) override {
    // Cancel is the same as selecting nothing.
    OnSelected({});
  }

  // Invokes |select_callback_| with the list of files and deletes this object.
  void OnSelected(const std::vector<base::FilePath>& paths) {
    std::vector<lacros::mojom::SelectedFileInfoPtr> files;
    for (const auto& path : paths) {
      lacros::mojom::SelectedFileInfoPtr file =
          lacros::mojom::SelectedFileInfo::New();
      file->file_path = path;
      files.push_back(std::move(file));
    }
    std::move(select_callback_)
        .Run(lacros::mojom::SelectFileResult::kSuccess, std::move(files));
    delete this;
  }

  // Callback run after files are selected or the dialog is canceled.
  lacros::mojom::SelectFile::SelectCallback select_callback_;

  // The file select dialog.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace

// TODO(https://crbug.com/1090587): Connection error handling.
SelectFileImpl::SelectFileImpl(
    mojo::PendingReceiver<lacros::mojom::SelectFile> receiver)
    : receiver_(this, std::move(receiver)) {}

SelectFileImpl::~SelectFileImpl() = default;

void SelectFileImpl::Select(lacros::mojom::SelectFileOptionsPtr options,
                            SelectCallback callback) {
  // Deletes itself when the dialog closes.
  new SelectFileDialogHolder(std::move(options), std::move(callback));
}
