// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_data_source.h"

#include <limits>

#include "base/numerics/checked_math.h"

namespace {

int64_t CalculateBaseOffset(base::File* file) {
  if (file->IsValid())
    return file->Seek(base::File::FROM_CURRENT, 0);
  return 0;
}

size_t CalculateMaxBytes(base::File* file,
                         int64_t base_offset,
                         base::Optional<int64_t> max_bytes) {
  if (max_bytes)
    return *max_bytes;
  if (file->IsValid())
    return file->GetLength() - base_offset;
  return std::numeric_limits<size_t>::max();
}

}  // namespace

namespace mojo {

// static
MojoResult FileDataSource::ConvertFileErrorToMojoResult(
    base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return MOJO_RESULT_OK;
    case base::File::FILE_ERROR_NOT_FOUND:
      return MOJO_RESULT_NOT_FOUND;
    case base::File::FILE_ERROR_SECURITY:
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return MOJO_RESULT_PERMISSION_DENIED;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
    case base::File::FILE_ERROR_NO_MEMORY:
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    case base::File::FILE_ERROR_ABORT:
      return MOJO_RESULT_ABORTED;
    default:
      return MOJO_RESULT_UNKNOWN;
  }
}

FileDataSource::FileDataSource(base::File file,
                               base::Optional<int64_t> max_bytes)
    : file_(std::move(file)),
      base_offset_(CalculateBaseOffset(&file_)),
      max_bytes_(CalculateMaxBytes(&file_, base_offset_, max_bytes)) {}

FileDataSource::~FileDataSource() = default;

bool FileDataSource::IsValid() const {
  return file_.IsValid();
}

int64_t FileDataSource::GetLength() const {
  return max_bytes_ - base_offset_;
}

DataPipeProducer::DataSource::ReadResult FileDataSource::Read(
    int64_t offset,
    base::span<char> buffer) {
  ReadResult result;
  size_t readable_size = max_bytes_ - offset;
  size_t read_size = std::min(readable_size, buffer.size());
  DCHECK(base::IsValueInRangeForNumericType<int>(read_size));
  int bytes_read = file_.Read(base_offset_ + offset, buffer.data(), read_size);
  if (bytes_read < 0) {
    result.bytes_read = 0;
    result.result = ConvertFileErrorToMojoResult(file_.GetLastFileError());
  } else {
    result.bytes_read = bytes_read;
  }
  return result;
}

}  // namespace mojo
