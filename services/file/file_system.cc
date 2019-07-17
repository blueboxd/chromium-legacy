// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/file/file_system.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace file {

FileSystem::FileSystem(const base::FilePath& base_user_dir,
                       const scoped_refptr<filesystem::LockTable>& lock_table)
    : lock_table_(lock_table), path_(base_user_dir) {
  base::CreateDirectory(path_);
}

FileSystem::~FileSystem() = default;

void FileSystem::GetDirectory(
    mojo::PendingReceiver<filesystem::mojom::Directory> receiver,
    GetDirectoryCallback callback) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<filesystem::DirectoryImpl>(
          path_, scoped_refptr<filesystem::SharedTempDir>(), lock_table_),
      std::move(receiver));
  std::move(callback).Run();
}

void FileSystem::GetSubDirectory(
    const std::string& sub_directory_path,
    mojo::PendingReceiver<filesystem::mojom::Directory> receiver,
    GetSubDirectoryCallback callback) {
  // Ensure that we've made |subdirectory| recursively under our user dir.
  base::FilePath subdir = path_.Append(
#if defined(OS_WIN)
      base::UTF8ToWide(sub_directory_path));
#else
      sub_directory_path);
#endif
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(subdir, &error)) {
    std::move(callback).Run(error);
    return;
  }

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<filesystem::DirectoryImpl>(
          subdir, scoped_refptr<filesystem::SharedTempDir>(), lock_table_),
      std::move(receiver));
  std::move(callback).Run(base::File::Error::FILE_OK);
}

}  // namespace file
