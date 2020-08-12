// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/input_file.h"

#include "base/logging.h"

namespace location {
namespace nearby {
namespace chrome {

InputFile::InputFile(base::File file) : file_(std::move(file)) {
  DCHECK(file_.IsValid());
  seek_succeeded_ = file_.Seek(base::File::FROM_BEGIN, 0) == 0;
}

InputFile::~InputFile() = default;

std::string InputFile::GetFilePath() const {
  // File path is not supported.
  return std::string();
}

std::int64_t InputFile::GetTotalSize() const {
  return file_.GetLength();
}

ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  if (!seek_succeeded_)
    return Exception::kFailed;

  ByteArray bytes(size);
  int bytes_read = file_.ReadAtCurrentPos(bytes.data(), bytes.size());
  if (bytes_read != size)
    return Exception::kFailed;

  return ExceptionOr<ByteArray>(std::move(bytes));
}

Exception InputFile::Close() {
  file_.Close();
  return {Exception::kSuccess};
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
