// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_file_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/fileapi/file_system_types.h"

namespace content {

namespace {

// The CDM interface has a restriction that file names can not begin with _,
// so use it to prefix temporary files.
const char kTemporaryFilePrefix = '_';

// File size limit is 512KB. Licenses saved by the CDM are typically several
// hundreds of bytes.
const int64_t kMaxFileSizeBytes = 512 * 1024;

// Maximum length of a file name.
const size_t kFileNameMaxLength = 256;

std::string GetTempFileName(const std::string& file_name) {
  DCHECK(!base::StartsWith(file_name, std::string(1, kTemporaryFilePrefix),
                           base::CompareCase::SENSITIVE));
  return kTemporaryFilePrefix + file_name;
}

// The file system is different for each CDM and each origin. So track files
// in use based on (file system ID, origin, file name).
struct FileLockKey {
  FileLockKey(const std::string& file_system_id,
              const url::Origin& origin,
              const std::string& file_name)
      : file_system_id(file_system_id), origin(origin), file_name(file_name) {}
  ~FileLockKey() = default;

  // Allow use as a key in std::set.
  bool operator<(const FileLockKey& other) const {
    return std::tie(file_system_id, origin, file_name) <
           std::tie(other.file_system_id, other.origin, other.file_name);
  }

  std::string file_system_id;
  url::Origin origin;
  std::string file_name;
};

// File map shared by all CdmFileImpl objects to prevent read/write race.
// A lock must be acquired before opening a file to ensure that the file is not
// currently in use. The lock must be held until the file is closed.
class FileLockMap {
 public:
  FileLockMap() = default;
  ~FileLockMap() = default;

  // Acquire a lock on the file represented by |key|. Returns true if |key|
  // is not currently in use, false otherwise.
  bool AcquireFileLock(const FileLockKey& key) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Add a new entry. If |key| already has an entry, insert() tells so
    // with the second piece of the returned value and does not modify
    // the original.
    return file_lock_map_.insert(key).second;
  }

  // Release the lock held on the file represented by |key|.
  void ReleaseFileLock(const FileLockKey& key) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    auto entry = file_lock_map_.find(key);
    if (entry == file_lock_map_.end()) {
      NOTREACHED() << "Unable to release lock on file " << key.file_name;
      return;
    }

    file_lock_map_.erase(entry);
  }

 private:
  // Note that this map is never deleted. As entries are removed when a file
  // is closed, it should never get too large.
  std::set<FileLockKey> file_lock_map_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(FileLockMap);
};

// The FileLockMap is a global lock map shared by all CdmFileImpl instances.
FileLockMap* GetFileLockMap() {
  static auto* file_lock_map = new FileLockMap();
  return file_lock_map;
}

// Read the contents of |file| and return it. On success, returns kSuccess and
// |data| is updated with the contents of the file. On failure kFailure is
// returned. This method owns |file| and it will be closed at the end.
CdmFileImpl::Status ReadFile(base::File file, std::vector<uint8_t>* data) {
  DCHECK(data->empty());

  // Determine the size of the file (so we know how many bytes to read).
  // Negative bytes mean failure, so problem with the file.
  int64_t num_bytes = file.GetLength();
  if (num_bytes < 0) {
    DLOG(WARNING) << __func__
                  << " Unable to get file length. result = " << num_bytes;
    return CdmFileImpl::Status::kFailure;
  }

  // Files are limited in size, so fail if file too big.
  if (num_bytes > kMaxFileSizeBytes) {
    DLOG(WARNING) << __func__
                  << " Too much data to read. #bytes = " << num_bytes;
    return CdmFileImpl::Status::kFailure;
  }

  // If the file has 0 bytes, no need to read anything.
  if (num_bytes == 0) {
    return CdmFileImpl::Status::kSuccess;
  }

  // Read the contents of the file. Read() sizes (provided and returned) are
  // type int, so cast appropriately.
  int bytes_to_read = base::checked_cast<int>(num_bytes);
  data->resize(num_bytes);

  TRACE_EVENT0("media", "CdmFileReadFile");
  base::TimeTicks start = base::TimeTicks::Now();
  int bytes_read =
      file.Read(0, reinterpret_cast<char*>(data->data()), bytes_to_read);
  base::TimeDelta read_time = base::TimeTicks::Now() - start;
  if (bytes_to_read != bytes_read) {
    // Unable to read the contents of the file.
    DLOG(WARNING) << "Failed to read file. Requested " << bytes_to_read
                  << " bytes, got " << bytes_read;
    return CdmFileImpl::Status::kFailure;
  }

  // Only report reading time for successful reads.
  UMA_HISTOGRAM_TIMES("Media.EME.CdmFileIO.ReadTime", read_time);
  return CdmFileImpl::Status::kSuccess;
}

// Write |data| to |file|. Returns kSuccess if everything works, kFailure
// otherwise.  This method owns |file| and it will be closed at the end.
CdmFileImpl::Status WriteFile(base::File file, std::vector<uint8_t> data) {
  // As the temporary file should have been newly created, it should be empty.
  CHECK_EQ(0u, file.GetLength()) << "Temporary file is not empty.";
  int bytes_to_write = base::checked_cast<int>(data.size());

  TRACE_EVENT0("media", "CdmFileWriteFile");
  base::TimeTicks start = base::TimeTicks::Now();
  int bytes_written =
      file.Write(0, reinterpret_cast<const char*>(data.data()), bytes_to_write);
  base::TimeDelta write_time = base::TimeTicks::Now() - start;
  if (bytes_written != bytes_to_write) {
    DLOG(WARNING) << "Failed to write file. Requested " << bytes_to_write
                  << " bytes, wrote " << bytes_written;
    return CdmFileImpl::Status::kFailure;
  }

  // Only report writing time for successful writes.
  UMA_HISTOGRAM_TIMES("Media.EME.CdmFileIO.WriteTime", write_time);

  return CdmFileImpl::Status::kSuccess;
}

}  // namespace

// static
bool CdmFileImpl::IsValidName(const std::string& name) {
  // File names must only contain letters (A-Za-z), digits(0-9), or "._-",
  // and not start with "_". It must contain at least 1 character, and not
  // more then |kFileNameMaxLength| characters.
  if (name.empty() || name.length() > kFileNameMaxLength ||
      name[0] == kTemporaryFilePrefix) {
    return false;
  }

  for (const auto ch : name) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '.' &&
        ch != '_' && ch != '-') {
      return false;
    }
  }

  return true;
}

CdmFileImpl::CdmFileImpl(
    const std::string& file_name,
    const url::Origin& origin,
    const std::string& file_system_id,
    const std::string& file_system_root_uri,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : file_name_(file_name),
      temp_file_name_(GetTempFileName(file_name_)),
      origin_(origin),
      file_system_id_(file_system_id),
      file_system_root_uri_(file_system_root_uri),
      file_system_context_(file_system_context),
      weak_factory_(this) {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK(IsValidName(file_name_));
}

CdmFileImpl::~CdmFileImpl() {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (file_locked_)
    ReleaseFileLock(file_name_);
}

bool CdmFileImpl::Initialize() {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!file_locked_);

  // Grab the lock on |file_name_|. The lock will be held until this object is
  // destructed.
  if (!AcquireFileLock(file_name_)) {
    DVLOG(2) << "File " << file_name_ << " is already in use.";
    return false;
  }

  // We have the lock on |file_name_|. |file_locked_| is set to simplify
  // validation, and to help destruction not have to check.
  file_locked_ = true;
  return true;
}

void CdmFileImpl::Read(ReadCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);
  DCHECK(data_.empty());

  // Open the file for reading. This may fail if the file does not currently
  // exist, which needs to be handled.
  OpenFile(file_name_, base::File::FLAG_OPEN | base::File::FLAG_READ,
           base::BindOnce(&CdmFileImpl::OnFileOpenedForReading,
                          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CdmFileImpl::OpenFile(const std::string& file_name,
                           uint32_t file_flags,
                           CreateOrOpenCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name << ", flags: " << file_flags;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  storage::FileSystemURL file_url = CreateFileSystemURL(file_name);
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());
  operation_context->set_allowed_bytes_growth(storage::QuotaManager::kNoLimit);
  DVLOG(3) << "Opening " << file_url.DebugString();

  file_util->CreateOrOpen(std::move(operation_context), file_url, file_flags,
                          std::move(callback));
}

void CdmFileImpl::OnFileOpenedForReading(ReadCallback callback,
                                         base::File file,
                                         base::OnceClosure on_close_callback) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (!file.IsValid()) {
    // File is invalid, so assume that it is empty.
    DVLOG(2) << "Unable to open file " << file_name_
             << ", error: " << base::File::ErrorToString(file.error_details());
    std::move(callback).Run(
        file.error_details() == base::File::FILE_ERROR_NOT_FOUND
            ? Status::kSuccess
            : Status::kFailure,
        std::vector<uint8_t>());
    return;
  }

  // Reading |file| must be done on a thread that allows blocking, so post a
  // task to do the read on a separate thread. When that completes simply call
  // |callback| with the results.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadFile, std::move(file), &data_),
      base::BindOnce(&CdmFileImpl::OnFileRead, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileRead(ReadCallback callback, Status status) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  std::vector<uint8_t> data;
  data.swap(data_);
  std::move(callback).Run(status, std::move(data));
}

void CdmFileImpl::Write(const std::vector<uint8_t>& data,
                        WriteCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name_ << ", size: " << data.size();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);
  DCHECK(data_.empty());

  // If there is no data to write, delete the file to save space.
  if (data.empty()) {
    DeleteFile(std::move(callback));
    return;
  }

  // Files are limited in size, so fail if file too big. This should have been
  // checked by the caller, but we don't fully trust IPC.
  if (data.size() > kMaxFileSizeBytes) {
    DLOG(WARNING) << __func__
                  << " Too much data to write. #bytes = " << data.size();
    std::move(callback).Run(Status::kFailure);
    return;
  }

  // Open the temporary file for writing. Specifying FLAG_CREATE_ALWAYS which
  // will overwrite any existing file.
  OpenFile(
      temp_file_name_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE,
      base::BindOnce(&CdmFileImpl::OnTempFileOpenedForWriting,
                     weak_factory_.GetWeakPtr(), data, std::move(callback)));
}

void CdmFileImpl::OnTempFileOpenedForWriting(
    std::vector<uint8_t> data,
    WriteCallback callback,
    base::File file,
    base::OnceClosure on_close_callback) {
  DVLOG(3) << __func__ << " file: " << temp_file_name_
           << ", bytes_to_write: " << data.size();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (!file.IsValid()) {
    DLOG(WARNING) << "Unable to open file " << temp_file_name_ << ", error: "
                  << base::File::ErrorToString(file.error_details());
    std::move(callback).Run(Status::kFailure);
    return;
  }

  // Writing to |file| must be done on a thread that allows blocking, so post a
  // task to do the writing on a separate thread. When that completes we need to
  // rename the file in order to replace any existing contents.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&WriteFile, std::move(file), std::move(data)),
      base::BindOnce(&CdmFileImpl::OnFileWritten, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileWritten(WriteCallback callback, Status status) {
  DVLOG(3) << __func__ << " file: " << temp_file_name_
           << ", status: " << status;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (status != Status::kSuccess) {
    // Write failed, so fail.
    std::move(callback).Run(status);
    return;
  }

  // Now rename |temp_file_name_| to |file_name_|.
  storage::FileSystemURL src_file_url = CreateFileSystemURL(temp_file_name_);
  storage::FileSystemURL dest_file_url = CreateFileSystemURL(file_name_);
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());
  DVLOG(3) << "Renaming " << src_file_url.DebugString() << " to "
           << dest_file_url.DebugString();
  file_util->MoveFileLocal(
      std::move(operation_context), src_file_url, dest_file_url,
      storage::FileSystemOperation::OPTION_NONE,
      base::BindOnce(&CdmFileImpl::OnFileRenamed, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileRenamed(WriteCallback callback,
                                base::File::Error move_result) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  // Was the rename successful?
  if (move_result != base::File::FILE_OK) {
    DLOG(WARNING) << "Unable to rename file " << temp_file_name_ << " to "
                  << file_name_
                  << ", error: " << base::File::ErrorToString(move_result);
    std::move(callback).Run(Status::kFailure);
    return;
  }

  std::move(callback).Run(Status::kSuccess);
}

void CdmFileImpl::DeleteFile(WriteCallback callback) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  storage::FileSystemURL file_url = CreateFileSystemURL(file_name_);
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());

  DVLOG(3) << "Deleting " << file_url.DebugString();
  file_util->DeleteFile(
      std::move(operation_context), file_url,
      base::BindOnce(&CdmFileImpl::OnFileDeleted, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileDeleted(WriteCallback callback,
                                base::File::Error result) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (result != base::File::FILE_OK &&
      result != base::File::FILE_ERROR_NOT_FOUND) {
    DLOG(WARNING) << "Unable to delete file " << file_name_
                  << ", error: " << base::File::ErrorToString(result);
    std::move(callback).Run(Status::kFailure);
    return;
  }

  std::move(callback).Run(Status::kSuccess);
}

storage::FileSystemURL CdmFileImpl::CreateFileSystemURL(
    const std::string& file_name) {
  return file_system_context_->CrackURL(
      GURL(file_system_root_uri_ + file_name));
}

bool CdmFileImpl::AcquireFileLock(const std::string& file_name) {
  FileLockKey file_lock_key(file_system_id_, origin_, file_name);
  return GetFileLockMap()->AcquireFileLock(file_lock_key);
}

void CdmFileImpl::ReleaseFileLock(const std::string& file_name) {
  FileLockKey file_lock_key(file_system_id_, origin_, file_name);
  GetFileLockMap()->ReleaseFileLock(file_lock_key);
}

}  // namespace content
