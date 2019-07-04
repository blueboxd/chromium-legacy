// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "net/base/escape.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

using blink::mojom::NativeFileSystemEntry;
using blink::mojom::NativeFileSystemEntryPtr;
using blink::mojom::NativeFileSystemError;
using blink::mojom::NativeFileSystemHandle;
using blink::mojom::NativeFileSystemTransferTokenPtr;
using blink::mojom::NativeFileSystemTransferTokenRequest;

namespace content {

namespace {

// Returns true when |name| contains a path separator like "/".
bool ContainsPathSeparator(const std::string& name) {
  const base::FilePath filepath_name = storage::StringToFilePath(name);

  const size_t separator_position =
      filepath_name.value().find_first_of(base::FilePath::kSeparators);

  return separator_position != base::FilePath::StringType::npos;
}

// Returns true when |name| is "." or "..".
bool IsCurrentOrParentDirectory(const std::string& name) {
  const base::FilePath filepath_name = storage::StringToFilePath(name);
  return filepath_name.value() == base::FilePath::kCurrentDirectory ||
         filepath_name.value() == base::FilePath::kParentDirectory;
}

}  // namespace

struct NativeFileSystemDirectoryHandleImpl::ReadDirectoryState {
  GetEntriesCallback callback;
  std::vector<NativeFileSystemEntryPtr> entries;
};

NativeFileSystemDirectoryHandleImpl::NativeFileSystemDirectoryHandleImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 handle_state,
                                 /*is_directory=*/true) {}

NativeFileSystemDirectoryHandleImpl::~NativeFileSystemDirectoryHandleImpl() =
    default;

void NativeFileSystemDirectoryHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DoGetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemDirectoryHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DoRequestPermission(writable, std::move(callback));
}

void NativeFileSystemDirectoryHandleImpl::GetFile(const std::string& basename,
                                                  bool create,
                                                  GetFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::FileSystemURL child_url;
  const base::File::Error file_error = GetChildURL(basename, &child_url);
  if (file_error != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(file_error), nullptr);
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
        nullptr);
    return;
  }

  if (create) {
    // If |create| is true, write permission is required unconditionally, i.e.
    // even if the file already exists. This is intentional, and matches the
    // behavior that is specified in the spec.
    RunWithWritePermission(
        base::BindOnce(
            &NativeFileSystemDirectoryHandleImpl::GetFileWithWritePermission,
            weak_factory_.GetWeakPtr(), child_url),
        base::BindOnce([](GetFileCallback callback) {
          std::move(callback).Run(
              NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
              nullptr);
        }),
        std::move(callback));
  } else {
    operation_runner()->FileExists(
        child_url,
        base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetFile,
                       weak_factory_.GetWeakPtr(), child_url,
                       std::move(callback)));
  }
}

void NativeFileSystemDirectoryHandleImpl::GetDirectory(
    const std::string& basename,
    bool create,
    GetDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::FileSystemURL child_url;
  const base::File::Error file_error = GetChildURL(basename, &child_url);
  if (file_error != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(file_error), nullptr);
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
        nullptr);
    return;
  }

  if (create) {
    // If |create| is true, write permission is required unconditionally, i.e.
    // even if the file already exists. This is intentional, and matches the
    // behavior that is specified in the spec.
    RunWithWritePermission(
        base::BindOnce(&NativeFileSystemDirectoryHandleImpl::
                           GetDirectoryWithWritePermission,
                       weak_factory_.GetWeakPtr(), child_url),
        base::BindOnce([](GetDirectoryCallback callback) {
          std::move(callback).Run(
              NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
              nullptr);
        }),
        std::move(callback));
  } else {
    operation_runner()->DirectoryExists(
        child_url,
        base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetDirectory,
                       weak_factory_.GetWeakPtr(), child_url,
                       std::move(callback)));
  }
}

void NativeFileSystemDirectoryHandleImpl::GetEntries(
    GetEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  operation_runner()->ReadDirectory(
      url(), base::BindRepeating(
                 &NativeFileSystemDirectoryHandleImpl::DidReadDirectory,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(new ReadDirectoryState{std::move(callback)})));
}

void NativeFileSystemDirectoryHandleImpl::RemoveEntry(
    const std::string& basename,
    bool recurse,
    RemoveEntryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::FileSystemURL child_url;
  const base::File::Error file_error = GetChildURL(basename, &child_url);
  if (file_error != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(file_error));
    return;
  }

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::RemoveEntryImpl,
                     weak_factory_.GetWeakPtr(), child_url, recurse),
      base::BindOnce([](RemoveEntryCallback callback) {
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED));
      }),
      std::move(callback));
}

void NativeFileSystemDirectoryHandleImpl::Transfer(
    NativeFileSystemTransferTokenRequest token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  manager()->CreateTransferToken(*this, std::move(token));
}

void NativeFileSystemDirectoryHandleImpl::GetFileWithWritePermission(
    const storage::FileSystemURL& child_url,
    GetFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  operation_runner()->CreateFile(
      child_url, /*exclusive=*/false,
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetFile,
                     weak_factory_.GetWeakPtr(), child_url,
                     std::move(callback)));
}

void NativeFileSystemDirectoryHandleImpl::DidGetFile(
    const storage::FileSystemURL& url,
    GetFileCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(result), nullptr);
    return;
  }

  std::move(callback).Run(
      NativeFileSystemError::New(base::File::FILE_OK),
      manager()->CreateFileHandle(context(), url, handle_state()));
}

void NativeFileSystemDirectoryHandleImpl::GetDirectoryWithWritePermission(
    const storage::FileSystemURL& child_url,
    GetDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  operation_runner()->CreateDirectory(
      child_url, /*exclusive=*/false, /*recursive=*/false,
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetDirectory,
                     weak_factory_.GetWeakPtr(), child_url,
                     std::move(callback)));
}

void NativeFileSystemDirectoryHandleImpl::DidGetDirectory(
    const storage::FileSystemURL& url,
    GetDirectoryCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(result), nullptr);
    return;
  }

  std::move(callback).Run(
      NativeFileSystemError::New(base::File::FILE_OK),
      manager()->CreateDirectoryHandle(context(), url, handle_state()));
}

void NativeFileSystemDirectoryHandleImpl::DidReadDirectory(
    ReadDirectoryState* state,
    base::File::Error result,
    std::vector<filesystem::mojom::DirectoryEntry> file_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    DCHECK(!has_more);
    std::move(state->callback).Run(NativeFileSystemError::New(result), {});
    return;
  }

  for (const auto& entry : file_list) {
    std::string basename = storage::FilePathToString(entry.name);

    storage::FileSystemURL child_url;
    const base::File::Error file_error = GetChildURL(basename, &child_url);

    // All entries must exist in this directory as a direct child with a valid
    // |basename|.
    CHECK_EQ(file_error, base::File::FILE_OK);

    state->entries.push_back(
        CreateEntry(basename, child_url,
                    entry.type == filesystem::mojom::FsFileType::DIRECTORY));
  }

  // TODO(mek): Change API so we can stream back entries as they come in, rather
  // than waiting till we have retrieved them all.
  if (!has_more) {
    std::move(state->callback)
        .Run(NativeFileSystemError::New(base::File::FILE_OK),
             std::move(state->entries));
  }
}

void NativeFileSystemDirectoryHandleImpl::RemoveEntryImpl(
    const storage::FileSystemURL& url,
    bool recurse,
    RemoveEntryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  operation_runner()->Remove(
      url, recurse,
      base::BindOnce(
          [](RemoveEntryCallback callback, base::File::Error result) {
            std::move(callback).Run(NativeFileSystemError::New(result));
          },
          std::move(callback)));
}

base::File::Error NativeFileSystemDirectoryHandleImpl::GetChildURL(
    const std::string& basename,
    storage::FileSystemURL* result) {
  // TODO(mek): Rather than doing URL serialization and parsing we should just
  // have a way to get a child FileSystemURL directly from its parent.

  if (basename.empty()) {
    return base::File::FILE_ERROR_NOT_FOUND;
  }

  if (ContainsPathSeparator(basename) || IsCurrentOrParentDirectory(basename)) {
    // |basename| must refer to a entry that exists in this directory as a
    // direct child.
    return base::File::FILE_ERROR_SECURITY;
  }

  std::string escaped_name =
      net::EscapeQueryParamValue(basename, /*use_plus=*/false);

  GURL parent_url = url().ToGURL();
  std::string path = base::StrCat({parent_url.path(), "/", escaped_name});
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  GURL child_url = parent_url.ReplaceComponents(replacements);

  *result = file_system_context()->CrackURL(child_url);
  return base::File::FILE_OK;
}

NativeFileSystemEntryPtr NativeFileSystemDirectoryHandleImpl::CreateEntry(
    const std::string& basename,
    const storage::FileSystemURL& url,
    bool is_directory) {
  if (is_directory) {
    return NativeFileSystemEntry::New(
        NativeFileSystemHandle::NewDirectory(
            manager()
                ->CreateDirectoryHandle(context(), url, handle_state())
                .PassInterface()),
        basename);
  }
  return NativeFileSystemEntry::New(
      NativeFileSystemHandle::NewFile(
          manager()
              ->CreateFileHandle(context(), url, handle_state())
              .PassInterface()),
      basename);
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemDirectoryHandleImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
