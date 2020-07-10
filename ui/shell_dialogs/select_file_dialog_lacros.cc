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
#include "ui/shell_dialogs/selected_file_info.h"

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

lacros::mojom::AllowedPaths GetMojoAllowedPaths(
    SelectFileDialog::FileTypeInfo::AllowedPaths allowed_paths) {
  switch (allowed_paths) {
    case SelectFileDialog::FileTypeInfo::ANY_PATH:
      return lacros::mojom::AllowedPaths::kAnyPath;
    case SelectFileDialog::FileTypeInfo::NATIVE_PATH:
      return lacros::mojom::AllowedPaths::kNativePath;
    case SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL:
      return lacros::mojom::AllowedPaths::kAnyPathOrUrl;
  }
}

SelectedFileInfo ConvertSelectedFileInfo(
    lacros::mojom::SelectedFileInfoPtr mojo_file) {
  SelectedFileInfo file;
  file.file_path = std::move(mojo_file->file_path);
  file.local_path = std::move(mojo_file->local_path);
  file.display_name = std::move(mojo_file->display_name);
  file.url = std::move(mojo_file->url);
  return file;
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

  lacros::mojom::SelectFileOptionsPtr options =
      lacros::mojom::SelectFileOptions::New();
  options->type = GetMojoType(type);
  options->title = title;
  options->default_path = default_path;
  if (file_types) {
    options->file_types = lacros::mojom::SelectFileTypeInfo::New();
    options->file_types->extensions = file_types->extensions;
    options->file_types->extension_description_overrides =
        file_types->extension_description_overrides;
    // NOTE: Index is 1-based, 0 means "no selection".
    options->file_types->default_file_type_index = file_type_index;
    options->file_types->include_all_files = file_types->include_all_files;
    options->file_types->allowed_paths =
        GetMojoAllowedPaths(file_types->allowed_paths);
  }

  // Send request to ash-chrome.
  chromeos::LacrosChromeServiceImpl::Get()->select_file_remote()->Select(
      std::move(options),
      base::BindOnce(&SelectFileDialogLacros::OnSelected, this));
}

void SelectFileDialogLacros::OnSelected(
    lacros::mojom::SelectFileResult result,
    std::vector<lacros::mojom::SelectedFileInfoPtr> mojo_files,
    int file_type_index) {
  if (!listener_)
    return;
  if (mojo_files.empty()) {
    listener_->FileSelectionCanceled(params_);
    return;
  }
  if (mojo_files.size() == 1) {
    SelectedFileInfo file = ConvertSelectedFileInfo(std::move(mojo_files[0]));
    listener_->FileSelectedWithExtraInfo(file, file_type_index, params_);
    return;
  }
  std::vector<SelectedFileInfo> files;
  for (auto& mojo_file : mojo_files) {
    files.push_back(ConvertSelectedFileInfo(std::move(mojo_file)));
  }
  listener_->MultiFilesSelectedWithExtraInfo(files, params_);
}

}  // namespace ui
