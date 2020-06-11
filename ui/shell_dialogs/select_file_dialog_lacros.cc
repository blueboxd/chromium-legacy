// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_lacros.h"

#include "base/bind.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

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
  // TODO(https://crbug.com/1090587): Proxy the request over IPC to ash-chrome.
  NOTIMPLEMENTED();
  // Until we have an implementation, pretend the user cancelled the dialog.
  // Post a task to avoid reentrancy issues. |this| is ref-counted.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&SelectFileDialogLacros::Cancel, this, params));
}

void SelectFileDialogLacros::Cancel(void* params) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
}

}  // namespace ui
