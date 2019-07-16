// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_data_source.h"

#include "base/numerics/checked_math.h"

namespace mojo {

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
    result.error = file_.GetLastFileError();
  } else {
    result.bytes_read = bytes_read;
  }
  return result;
}

}  // namespace mojo
