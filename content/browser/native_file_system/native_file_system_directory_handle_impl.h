// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/browser/native_file_system/native_file_system_handle_base.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom.h"

namespace content {
// This is the browser side implementation of the
// NativeFileSystemDirectoryHandle mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods should be called on the same
// sequence as storage::FileSystemContext, which today always is the IO thread.
class NativeFileSystemDirectoryHandleImpl
    : public NativeFileSystemHandleBase,
      public blink::mojom::NativeFileSystemDirectoryHandle {
 public:
  NativeFileSystemDirectoryHandleImpl(NativeFileSystemManagerImpl* manager,
                                      const BindingContext& context,
                                      const storage::FileSystemURL& url,
                                      const SharedHandleState& handle_state);
  ~NativeFileSystemDirectoryHandleImpl() override;

  // blink::mojom::NativeFileSystemDirectoryHandle:
  void GetPermissionStatus(bool writable,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(bool writable,
                         RequestPermissionCallback callback) override;
  void GetFile(const std::string& name,
               bool create,
               GetFileCallback callback) override;
  void GetDirectory(const std::string& name,
                    bool create,
                    GetDirectoryCallback callback) override;
  void GetEntries(GetEntriesCallback callback) override;
  void Remove(bool recurse, RemoveCallback callback) override;
  void Transfer(
      blink::mojom::NativeFileSystemTransferTokenRequest token) override;

 private:
  // State that is kept for the duration of a GetEntries/ReadDirectory call.
  struct ReadDirectoryState;

  // This method creates the file if it does not currently exists. I.e. it is
  // the implementation for passing create=true to GetFile.
  void GetFileWithWritePermission(const storage::FileSystemURL& child_url,
                                  GetFileCallback callback);
  void DidGetFile(const storage::FileSystemURL& url,
                  GetFileCallback callback,
                  base::File::Error result);
  // This method creates the directory if it does not currently exists. I.e. it
  // is the implementation for passing create=true to GetDirectory.
  void GetDirectoryWithWritePermission(const storage::FileSystemURL& child_url,
                                       GetDirectoryCallback callback);
  void DidGetDirectory(const storage::FileSystemURL& url,
                       GetDirectoryCallback callback,
                       base::File::Error result);
  void DidReadDirectory(
      ReadDirectoryState* state,
      base::File::Error result,
      std::vector<filesystem::mojom::DirectoryEntry> file_list,
      bool has_more);

  void RemoveImpl(bool recurse, RemoveCallback callback);

  // Calculates a FileSystemURL for a (direct) child of this directory with the
  // given name.  Returns an error when |name| includes invalid input like "/".
  base::File::Error GetChildURL(const std::string& name,
                                storage::FileSystemURL* result)
      WARN_UNUSED_RESULT;

  // Helper to create a blink::mojom::NativeFileSystemEntry struct.
  blink::mojom::NativeFileSystemEntryPtr CreateEntry(
      const std::string& name,
      const storage::FileSystemURL& url,
      bool is_directory);

  base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<NativeFileSystemDirectoryHandleImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemDirectoryHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_
