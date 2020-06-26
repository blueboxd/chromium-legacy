// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_lacros.h"

#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chromeos/lacros/browser/lacros_chrome_service_impl.h"
#include "chromeos/lacros/mojom/select_file.mojom.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {
namespace {

lacros::mojom::SelectFileDialogType GetMojoType(SelectFileDialog::Type type) {
  switch (type) {
    case SelectFileDialog::Type::SELECT_FOLDER:
      return lacros::mojom::SelectFileDialogType::kFolder;
    case SelectFileDialog::Type::SELECT_UPLOAD_FOLDER:
      return lacros::mojom::SelectFileDialogType::kUploadFolder;
    case SelectFileDialog::Type::SELECT_EXISTING_FOLDER:
      return lacros::mojom::SelectFileDialogType::kExistingFolder;
    case SelectFileDialog::Type::SELECT_OPEN_FILE:
      return lacros::mojom::SelectFileDialogType::kOpenFile;
    case SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE:
      return lacros::mojom::SelectFileDialogType::kOpenMultiFile;
    case SelectFileDialog::Type::SELECT_SAVEAS_FILE:
      return lacros::mojom::SelectFileDialogType::kSaveAsFile;
    case SelectFileDialog::Type::SELECT_NONE:
      NOTREACHED();
      return lacros::mojom::SelectFileDialogType::kOpenFile;
  }
}

}  // namespace

SelectFileDialogLacros::Factory::Factory() = default;
SelectFileDialogLacros::Factory::~Factory() = default;

ui::SelectFileDialog* SelectFileDialogLacros::Factory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new SelectFileDialogLacros(listener, std::move(policy));
}

SelectFileDialogLacros::SelectFileDialogLacros(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : ui::SelectFileDialog(listener, std::move(policy)) {}

SelectFileDialogLacros::~SelectFileDialogLacros() = default;

bool SelectFileDialogLacros::HasMultipleFileTypeChoicesImpl() {
  return true;
}

bool SelectFileDialogLacros::IsRunning(gfx::NativeWindow owning_window) const {
  return true;
}

void SelectFileDialogLacros::SelectFileImpl(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  params_ = params;

  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  // TODO(https://crbug.com/1090587): Move LacrosChromeServiceImpl construction
  // earlier and remove these checks. This function is racy with lacros-chrome
  // startup. In practice, however, the remote is bound long before the user
  // can trigger a select dialog.
  if (!lacros_chrome_service ||
      !lacros_chrome_service->select_file_remote().is_bound()) {
    LOG(ERROR) << "Not connected to ash-chrome.";
    return;
  }

  lacros::mojom::SelectFileOptionsPtr options =
      lacros::mojom::SelectFileOptions::New();
  options->type = GetMojoType(type);
  options->title = title;
  options->default_path = default_path;

  // Send request to ash-chrome.
  lacros_chrome_service->select_file_remote()->Select(
      std::move(options),
      base::BindOnce(&SelectFileDialogLacros::OnSelected, this));
}

void SelectFileDialogLacros::OnSelected(
    lacros::mojom::SelectFileResult result,
    std::vector<lacros::mojom::SelectedFileInfoPtr> files) {
  if (!listener_)
    return;
  if (files.empty()) {
    listener_->FileSelectionCanceled(params_);
    return;
  }
  if (files.size() == 1) {
    // TODO(jamescook): Support correct file filter |index|.
    // TODO(jamescook): Use FileSelectedWithExtraInfo instead.
    listener_->FileSelected(files[0]->file_path, /*index=*/0, params_);
    return;
  }
  std::vector<base::FilePath> paths;
  for (auto& file : files)
    paths.push_back(std::move(file->file_path));
  // TODO(jamescook): Use MultiFilesSelectedWithExtraInfo instead.
  listener_->MultiFilesSelected(paths, params_);
}

}  // namespace ui
