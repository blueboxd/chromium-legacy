// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/interfaces/cdm_storage.mojom.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "url/origin.h"

namespace storage {
class FileSystemContext;
class FileSystemURL;
}  // namespace storage

namespace content {

// This class implements the media::mojom::CdmFile interface. It uses the same
// mojo pipe as CdmStorageImpl, to enforce message dispatch order.
class CdmFileImpl final : public media::mojom::CdmFile {
 public:
  // Check whether |name| is valid as a usable file name. Returns true if it is,
  // false otherwise.
  static bool IsValidName(const std::string& name);

  CdmFileImpl(const std::string& file_name,
              const url::Origin& origin,
              const std::string& file_system_id,
              const std::string& file_system_root_uri,
              scoped_refptr<storage::FileSystemContext> file_system_context);
  ~CdmFileImpl() final;

  // Called to grab a lock on the file. Returns false if the file is in use by
  // other CDMs or by the system, true otherwise. Note that |this| should not
  // be used anymore if Initialize() fails.
  bool Initialize();

  // media::mojom::CdmFile implementation.
  void Read(ReadCallback callback) final;
  void Write(const std::vector<uint8_t>& data, WriteCallback callback) final;

 private:
  using CreateOrOpenCallback = storage::AsyncFileUtil::CreateOrOpenCallback;

  // Open the file |file_name| using the flags provided in |file_flags|.
  // |callback| is called with the result.
  void OpenFile(const std::string& file_name,
                uint32_t file_flags,
                CreateOrOpenCallback callback);

  // Called when the file has been opened for reading, so it reads the contents
  // of |file| and passes them to |callback|. |file| is closed after reading.
  void OnFileOpenedForReading(ReadCallback callback,
                              base::File file,
                              base::OnceClosure on_close_callback);
  void OnFileRead(ReadCallback callback, Status status);

  // Called when |temp_file_name_| has been opened for writing. Writes
  // |data| to |file|, closes |file|, and then kicks off a rename of
  // |temp_file_name_| to |file_name_|, effectively replacing the contents of
  // the old file.
  void OnTempFileOpenedForWriting(std::vector<uint8_t> data,
                                  WriteCallback callback,
                                  base::File file,
                                  base::OnceClosure on_close_callback);
  void OnFileWritten(WriteCallback callback, Status status);
  void OnFileRenamed(WriteCallback callback, base::File::Error move_result);

  // Deletes |file_name_| asynchronously.
  void DeleteFile(WriteCallback callback);
  void OnFileDeleted(WriteCallback callback, base::File::Error result);

  // Returns the FileSystemURL for the specified |file_name|.
  storage::FileSystemURL CreateFileSystemURL(const std::string& file_name);

  // Helper methods to lock and unlock a file.
  bool AcquireFileLock(const std::string& file_name);
  void ReleaseFileLock(const std::string& file_name);

  // Names of the files this class represents.
  const std::string file_name_;
  const std::string temp_file_name_;

  // Files are stored in the PluginPrivateFileSystem. The following are needed
  // to access files.
  const url::Origin origin_;
  const std::string file_system_id_;
  const std::string file_system_root_uri_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Keep track of when the original file |file_name_| is locked.
  // Initialize() can only be called if false and takes the lock (on success).
  // Read() and Write() can only be called if true.
  // Note that having a lock on |file_name| implies that |temp_file_name| is
  // reserved for use by this object only, and an explicit lock on
  // |temp_file_name| is not required.
  bool file_locked_ = false;

  // Buffer used when reading the file.
  std::vector<uint8_t> data_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CdmFileImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmFileImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_
