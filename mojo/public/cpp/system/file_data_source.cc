// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_data_source.h"

#include "base/numerics/checked_math.h"

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

FileDataSource::FileDataSource(base::File file, size_t max_bytes)
    : file_(std::move(file)), max_bytes_(max_bytes) {
  base_offset_ = file_.IsValid() ? file_.Seek(base::File::FROM_CURRENT, 0) : 0;
}
FileDataSource::~FileDataSource() = default;

bool FileDataSource::IsValid() const {
  return file_.IsValid();
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
