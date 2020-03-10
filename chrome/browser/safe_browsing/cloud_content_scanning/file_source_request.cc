// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_source_request.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace safe_browsing {

namespace {

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileContentsForLargeFile(const base::FilePath& path, base::File* file) {
  size_t file_size = file->GetLength();
  BinaryUploadService::Request::Data file_data;
  file_data.size = file_size;

  // Only read 50MB at a time to avoid having very large files in memory.
  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  size_t bytes_read = 0;
  std::string buf;
  buf.reserve(BinaryUploadService::kMaxUploadSizeBytes);
  while (bytes_read < file_size) {
    int64_t bytes_currently_read = file->ReadAtCurrentPos(
        &buf[0], BinaryUploadService::kMaxUploadSizeBytes);

    if (bytes_currently_read == -1) {
      return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                            BinaryUploadService::Request::Data());
    }

    secure_hash->Update(buf.data(), bytes_currently_read);

    bytes_read += bytes_currently_read;
  }

  file_data.hash.resize(crypto::kSHA256Length);
  secure_hash->Finish(base::data(file_data.hash), crypto::kSHA256Length);
  file_data.hash =
      base::HexEncode(base::as_bytes(base::make_span(file_data.hash)));
  return std::make_pair(BinaryUploadService::Result::FILE_TOO_LARGE, file_data);
}

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileContentsForNormalFile(const base::FilePath& path, base::File* file) {
  size_t file_size = file->GetLength();
  BinaryUploadService::Request::Data file_data;
  file_data.size = file_size;
  file_data.contents.resize(file_size);

  int64_t bytes_currently_read =
      file->ReadAtCurrentPos(&file_data.contents[0], file_size);

  if (bytes_currently_read == -1) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                          BinaryUploadService::Request::Data());
  }

  DCHECK_EQ(static_cast<size_t>(bytes_currently_read), file_size);

  file_data.hash = crypto::SHA256HashString(file_data.contents);
  file_data.hash =
      base::HexEncode(base::as_bytes(base::make_span(file_data.hash)));
  return std::make_pair(BinaryUploadService::Result::SUCCESS, file_data);
}

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileDataBlocking(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                          BinaryUploadService::Request::Data());
  }

  return static_cast<size_t>(file.GetLength()) >
                 BinaryUploadService::kMaxUploadSizeBytes
             ? GetFileContentsForLargeFile(path, &file)
             : GetFileContentsForNormalFile(path, &file);
}

}  // namespace

FileSourceRequest::FileSourceRequest(base::FilePath path,
                                     BinaryUploadService::Callback callback)
    : Request(std::move(callback)),
      has_cached_result_(false),
      path_(std::move(path)) {
  set_filename(path_.BaseName().AsUTF8Unsafe());
}

FileSourceRequest::~FileSourceRequest() = default;

void FileSourceRequest::GetRequestData(DataCallback callback) {
  if (has_cached_result_) {
    std::move(callback).Run(cached_result_, cached_data_);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileDataBlocking, path_),
      base::BindOnce(&FileSourceRequest::OnGotFileData,
                     weakptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSourceRequest::OnGotFileData(
    DataCallback callback,
    std::pair<BinaryUploadService::Result, Data> result_and_data) {
  set_digest(result_and_data.second.hash);
  has_cached_result_ = true;
  cached_result_ = result_and_data.first;
  cached_data_ = result_and_data.second;
  std::move(callback).Run(result_and_data.first, result_and_data.second);
}

}  // namespace safe_browsing
